#include "MasterServer.h"

bool MasterServer::start() {
	Serial.println("Entering becomeMaster");

	WiFi.mode(WIFI_STA);

	if (!connectToWiFi(creds.load())) {
		Serial.println("failed to become master, could not connect to wifi");
		return false;
	}
	else Serial.println("I am the master");

	//Sets the ESP's output pin to OFF
	pinMode(GPIO_PIN, OUTPUT);
	digitalWrite(GPIO_PIN, HIGH);

	Serial.println("Opening soft access point...");
	WiFi.softAP(MASTER_INFO.ssid, MASTER_INFO.password); //Starts access point for new devices to connect to

	Serial.println("Adding endpoints...");
	addEndpoints();

	Serial.println("Adding unknown endpoint...");
	addUnknownEndpoint();

	Serial.println("Starting server...");
	server.begin(MASTER_PORT);

	Serial.println("Starting MDNS...");
	startMDNS();

	Serial.println("Enableing OTA updates...");
	enableOTAUpdates();

	Serial.println("Ready!");
	return true;
}

void MasterServer::update() {
	expireClientLookup();
	anotherMasterExists();
}

//Master endpoints
void MasterServer::addEndpoints() {
	for (std::vector<Endpoint>::iterator it = masterEndpoints.begin(); it != masterEndpoints.end(); ++it) {
		server.on(it->path, it->method, it->function);
	}
}
void MasterServer::addUnknownEndpoint()
{
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleMasterUnknown / masterToClientEndpoint");

		//If the request has no name/id or its name/id matches mine
		if (!validId() || isForMe()) {
			for (std::vector<Endpoint>::iterator it = clientEndpoints.begin(); it != clientEndpoints.end(); ++it) {
				if (server.uri().equals(it->path) && server.method() == it->method) {
					it->function();
					return;
				}
			}
			//When no matching endpoint can be found
			server.send(HTTP_CODE_NOT_FOUND, "application/json", "{\"error\":\"endpoint_missing\"}");
		}
		else reDirect();
	};
	server.onNotFound(lambda);
}
std::function<void()> MasterServer::handleMasterGetWiFiInfo() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleMasterGetWiFiInfo");

		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();

		WiFiInfo info = creds.load();
		json["ssid"] = info.ssid;
		json["password"] = info.password;
		json["master_ip"] = WiFi.localIP().toString();

		String content;
		json.printTo(content);

		server.send(HTTP_CODE_OK, "application/json", content);
	};

	return lambda;
}
std::function<void()> MasterServer::handleMasterGetDevices() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleMasterGetDevices");

		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();
		JsonArray& devices = json.createNestedArray("devices");

		devices.add(jsonBuffer.parseObject(getDeviceInfo()));

		for (std::unordered_set<Device>::iterator it = clientLookup.begin(); it != clientLookup.end(); ++it) {
			Device device = *it;
			JsonObject& jsonDevice = jsonBuffer.createObject();
			jsonDevice["id"] = device.id.toInt();
			jsonDevice["ip"] = device.ip;
			jsonDevice["name"] = device.name;
			devices.add(jsonDevice);
		}

		String result;
		json.printTo(result);

		server.send(HTTP_CODE_OK, "application/json", result);
	};

	return lambda;
}
std::function<void()> MasterServer::handleMasterCheckin() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleMasterCheckin");

		String ip = server.client().remoteIP().toString();

		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.parseObject(server.arg("plain"));
		if (!json.success() || !json.containsKey("id") || !json.containsKey("name")) {
			server.send(HTTP_CODE_BAD_REQUEST);
			return;
		}
		server.send(HTTP_CODE_OK);

		Device device = Device();
		device.id = json["id"].asString();
		device.ip = ip;
		device.name = json["name"].asString();

		std::unordered_set<Device>::const_iterator found = clientLookup.find(device);
		if (found != clientLookup.end()) found->timeout->reset();
		else clientLookup.insert(device);
	};

	return lambda;
}

//Master creation
void MasterServer::startMDNS() {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	json["id"] = ESP.getChipId();
	json["name"] = creds.load().hostname;
	String jsonName;
	json.printTo(jsonName);

	MDNS.close();
	MDNS.begin(jsonName.c_str());
	MDNS.addService(MASTER_MDNS_ID, "tcp", 80); //Broadcasts IP so can be seen by other devices
}
bool MasterServer::anotherMasterExists() {
	Serial.println("Checking for duplicate master...");

	//Initalises the chosen id to the id of the current device
	String myIp = WiFi.localIP().toString();
	String chosenIp = myIp;

	//Query for client devices
	int devicesFound = MDNS.queryService(MASTER_MDNS_ID, "tcp");
	for (int i = 0; i < devicesFound; ++i) {
		String currentIp = MDNS.answerIP(i).toString();
		if (currentIp > chosenIp) chosenIp = currentIp;
	}

	if (myIp.equals(chosenIp)) {
		Serial.println("I've been chosen to stay as master");
		MDNS.setHostname(MASTER_INFO.hostname);
		MDNS.notifyAPChange();
		MDNS.update();
	}
	else {
		Serial.println("Another master exists, turning into client");
		MDNS.close();
		ESP.restart();
	}
}

//REST request routing
String MasterServer::getDeviceIPFromIdOrName(String idOrName) {
	Serial.println("idOrName:" + idOrName);
	for (std::unordered_set<Device>::iterator it = clientLookup.begin(); it != clientLookup.end(); ++it) {
		if (it->id.equals(idOrName) || it->name.equals(idOrName)) return it->ip;
	}
	return "";
}
void MasterServer::reDirect() {
	String payload = server.arg("plain");

	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(payload);

	String idOrName = "";
	if (json.containsKey("id")) idOrName = json["id"].asString();
	else if (json.containsKey("name")) idOrName = json["name"].asString();

	String ip = getDeviceIPFromIdOrName(idOrName);
	if (ip == "") {
		String reply = "{\"error\":\"device_not_found\"}";
		server.send(HTTP_CODE_BAD_REQUEST, "application/json", reply);
		return;
	}

	HTTPClient http;
	http.setTimeout(2000); //Reduced the timeout from 5000 to fail faster
	http.begin("http://" + ip + ":" + CLIENT_PORT + server.uri());

	int httpCode = http.sendRequest(getMethod(), payload);

	//HttpCode is only below 0 when there is an issue contacting client
	if (httpCode < 0) {
		http.end();

		//If client can not be reached it is removed from the clientLookup
		for (std::unordered_set<Device>::iterator it = clientLookup.begin(); it != clientLookup.end(); ++it) {
			if (it->ip.equals(ip)) {
				clientLookup.erase(it);
				break;
			}
		}

		String reply = "{\"error\":\"error_contacting_device\"}";
		server.send(HTTP_CODE_BAD_GATEWAY, "application/json", reply);
		return;
	}
	else {
		String reply = http.getString();
		http.end();
		server.send(httpCode, "application/json", reply);
		return;
	}
}
String MasterServer::reDirect(String ip) {
	HTTPClient http;
	http.begin("http://" + ip + ":" + CLIENT_PORT + server.uri());

	String payload = "";
	if (server.hasArg("plain")) payload = server.arg("plain");

	int httpCode = http.sendRequest(getMethod(), payload);

	//HttpCode is only below 0 when there is an issue contacting client
	if (httpCode < 0) {
		http.end();
		return "error";
	}
	else {
		String reply = http.getString();
		http.end();
		return reply;
	}
}
void MasterServer::expireClientLookup() {
	
	
	// ### Can maybe move this logic into when the clientLookup is read from


	Serial.println("Handleing client expiring");
	for (std::unordered_set<Device>::iterator it = clientLookup.begin(); it != clientLookup.end(); ) {
		if (it->timeout->expired()) {
			it = clientLookup.erase(it);
		}
		else ++it;
	}
}

//Helper functions for REST routing
const char* MasterServer::getMethod() {
	const char* method;
	switch (server.method()) {
		case HTTP_ANY:     method = "GET";		break;
		case HTTP_GET:     method = "GET";		break;
		case HTTP_POST:    method = "POST";		break;
		case HTTP_PUT:     method = "PUT";		break;
		case HTTP_PATCH:   method = "PATCH";	break;
		case HTTP_DELETE:  method = "DELETE";	break;
		case HTTP_OPTIONS: method = "OPTIONS";	break;
	}
	return method;
}
bool MasterServer::isForMe() {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(server.arg("plain"));
	if (!json.success()) { server.send(HTTP_CODE_BAD_REQUEST); return false; }
	if (!json.containsKey("id") && !json.containsKey("name")) { server.send(HTTP_CODE_BAD_REQUEST); return false; }

	if (json.containsKey("id")) {
		String id = json["id"];
		String myId = (String)ESP.getChipId();
		return myId.equals(id);
	}
	else if (json.containsKey("name")) {
		String name = json["name"];
		WiFiInfo info = creds.load();
		String myName = info.hostname;
		return myName.equals(name);
	}
	else return false;
}
bool MasterServer::validId() {
	bool isValid = true;
	if (!server.hasArg("plain")) isValid = false;
	else {
		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.parseObject(server.arg("plain"));
		if (!json.success()) isValid = false;
		else if (!json.containsKey("id") && !json.containsKey("name")) isValid = false;
	}
	return isValid;
}
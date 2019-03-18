#include "MasterServer.h"

bool MasterServer::start() {
	Serial.println("Entering becomeMaster");

	WiFi.mode(WIFI_STA);

	if (!connectToWiFi(creds.load())) {
		Serial.println("failed to become master, could not connect to wifi");
		return false;
	}
	else Serial.println("I am a master!");

	//Sets the ESP's output pin to OFF
	pinMode(GPIO_PIN, OUTPUT);
	digitalWrite(GPIO_PIN, HIGH);

	//Starts access point for new devices to connect to
	Serial.println("Opening soft access point...");
	WiFi.softAP(MASTER_INFO.ssid, MASTER_INFO.password);

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
	checkForOtherMasters();
}

//Master endpoints
void MasterServer::addEndpoints() {
	for (std::vector<Endpoint>::iterator it = masterEndpoints.begin(); it != masterEndpoints.end(); ++it) {
		server.on(it->path, it->method, it->function);
	}
}
void MasterServer::addUnknownEndpoint() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleMasterUnknown / masterToClientEndpoint");

		//If the request has no name/id or its name/id matches mine
		if (!validId() || isForMe()) {
			//Call my client endpoint if it exists
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
std::function<void()> MasterServer::handleMasterSetCheckin() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleMasterCheckin");

		//Gets the IP from the client calling
		String ip = server.client().remoteIP().toString();

		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.parseObject(server.arg("plain"));

		if (!json.success()) { server.send(HTTP_CODE_BAD_REQUEST); return; }
		else if (!json.containsKey("id")) {
			server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"id_field_missing\"}");
			return;
		}
		else if (!json.containsKey("name")) {
			server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"name_field_missing\"}");
			return;
		}

		server.send(HTTP_CODE_OK);

		Device device = Device();
		device.id = json["id"].asString();
		device.ip = ip;
		device.name = json["name"].asString();

		std::unordered_set<Device>::const_iterator found = clientLookup.find(device);
		
		//If the device already exists in clientLookup just refresh it's timeout
		if (found != clientLookup.end()) {
			//If the device exists, but has a different name, replace it
			if (!found->name.equals(device.name)) {
				clientLookup.erase(found);
				clientLookup.insert(device);
			}
			else found->timeout->reset();
		}
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
void MasterServer::checkForOtherMasters() {
	Serial.println("Checking for duplicate master...");

	//Initalises the chosen id to the id of the current device
	String myId = String(ESP.getChipId());
	String chosenId = myId;

	std::vector<String> masterIPs;

	//Query for client devices
	int devicesFound = MDNS.queryService(MASTER_MDNS_ID, "tcp");
	for (int i = 0; i < devicesFound; ++i) {
		masterIPs.push_back(MDNS.answerIP(i).toString());
		String reply = MDNS.answerHostname(i);

		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.parseObject(reply);

		if (json.success() && json.containsKey("id")) {
			String currentId = json["id"].asString();

			if (currentId > chosenId) {
				chosenId = currentId;
			}
		}
	}

	if (myId.equals(chosenId)) {
		Serial.println("I've been chosen to stay as master");
		closeOtherMasters(masterIPs);
	}
	else {
		Serial.println("Another master exists, turning into client");
	}
}
void MasterServer::closeOtherMasters(std::vector<String> masterIPs) {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

	json["ip"] = WiFi.localIP().toString();

	String result;
	json.printTo(result);

	//Notifies all other masters that this device will be the only master
	for (std::vector<String>::iterator it = masterIPs.begin(); it != masterIPs.end(); ++it) {
		Serial.println("Letting know I'm the only master: " + *it);

		HTTPClient http;
		//Increase the timeout from 5000 to allow other clients to go through the electNewMaster steps
		http.setTimeout(7000);
		http.begin("http://" + *it + ":" + MASTER_PORT + "/restart");
		http.sendRequest("POST", result);
		http.end();
	}

	MDNS.setHostname(MASTER_INFO.hostname);
	MDNS.notifyAPChange();
	MDNS.update();
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
	String payload = "";
	if (!server.hasArg("plain")) payload = server.arg("plain");

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
	if (!server.hasArg("plain")) return false;

	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(server.arg("plain"));

	if (!json.success()) return false;
	else if (!json.containsKey("id") && !json.containsKey("name")) return false;
	else return true;
}
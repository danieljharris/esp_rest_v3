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

	MDNS.begin(MASTER_INFO.hostname); //Starts up MDNS
	WiFi.softAP(MASTER_INFO.ssid, MASTER_INFO.password); //Starts access point for new devices to connect to

	Serial.println("Enableing OTA updates...");
	enableOTAUpdates();

	Serial.println("Adding endpoints...");
	addEndpoints();

	Serial.println("Adding unknown endpoint...");
	addUnknownEndpoint();

	Serial.println("Starting server...");
	server.begin(MASTER_PORT);
	
	return true;
}

void MasterServer::addEndpoints() {
	std::function<void()> masterGetWiFiInfo = handleMasterGetWiFiInfo();
	std::function<void()> masterGetDevices = handleMasterGetDevices();
	std::function<void()> masterSetDevice = handleMasterSetDevice();
	std::function<void()> masterSetDeviceName = handleMasterSetDeviceName();
	std::function<void()> masterSetWiFiCreds = handleMasterSetWiFiCreds();

	server.on("/wifi_info", HTTP_GET, masterGetWiFiInfo);
	server.on("/device", HTTP_GET, masterGetDevices);
	server.on("/device", HTTP_POST, masterSetDevice);
	server.on("/name", HTTP_POST, masterSetDeviceName);
	server.on("/credentials", HTTP_POST, masterSetWiFiCreds);
}
void MasterServer::addUnknownEndpoint()
{
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleMasterUnknown");

		if (!validId()) return;

		if (isForMe()) server.send(HTTP_CODE_NOT_FOUND);
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

		String content;
		json.printTo(content);

		server.send(HTTP_CODE_OK, "application/json", content);
	};

	return lambda;
}
std::function<void()> MasterServer::handleMasterGetDevices() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleMasterGetDevices");

		refreshLookup();

		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();
		JsonArray& devices = json.createNestedArray("devices");

		String myIp = WiFi.localIP().toString();

		for (std::vector<Device>::iterator it = clientLookup.begin(); it != clientLookup.end(); ++it) {
			Device device = *it;

			if (device.ip.equals(myIp)) devices.add(jsonBuffer.parseObject(getDeviceInfo()));
			else {
				String reply = reDirect(device.ip);
				if (reply.equals("error") == false) {
					devices.add(jsonBuffer.parseObject(reply));
				}
			}
		}

		String result;
		json.printTo(result);

		server.send(HTTP_CODE_OK, "application/json", result);
	};

	return lambda;
}
std::function<void()> MasterServer::handleMasterSetDevice() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleMasterSetDevice");

		if (!validId()) return;

		if (isForMe()) handleClientSetDevice()();
		else reDirect();
	};

	return lambda;
}
std::function<void()> MasterServer::handleMasterSetDeviceName() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleMasterSetDeviceName");

		if (!validId()) return;

		if (isForMe()) handleClientSetName()();
		else reDirect();
	};

	return lambda;
}
std::function<void()> MasterServer::handleMasterSetWiFiCreds() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleMasterSetWiFiCreds");

		if (!validId()) return;

		if (isForMe()) handleClientSetWiFiCreds()();
		else reDirect();
	};

	return lambda;
}

void MasterServer::refreshLookup() {
	Serial.println("Entering refreshLookup");

	//Clears known clients ready to repopulate the vector
	clientLookup.clear();

	//Adds master (itself) to clientLookup
	WiFiInfo info = creds.load();
	Device myself;
	myself.id = (String)ESP.getChipId();
	myself.ip = WiFi.localIP().toString();
	myself.name = info.hostname;
	clientLookup.push_back(myself);

	//Send out query for ESP_REST devices
	int devicesFound = MDNS.queryService(MDNS_ID, "tcp");
	Serial.print("devicesFound: ");
	Serial.println(devicesFound);
	for (int i = 0; i < devicesFound; ++i) {
		//Call the device's IP and get its info
		String ip = MDNS.IP(i).toString();
		String reply = reDirect(ip);

		Serial.println("ip: " + ip);
		Serial.println("reply: " + reply);

		if (reply.equals("error") == false) {
			DynamicJsonBuffer jsonBuffer;
			JsonObject& input = jsonBuffer.parseObject(reply);

			Device newDevice;
			newDevice.ip = ip;
			newDevice.id = input["id"].asString();
			newDevice.name = input["name"].asString();

			clientLookup.push_back(newDevice);
		}
	}

	Serial.println("Leaving refreshLookup");
}

String MasterServer::getDeviceIPFromIdOrName(String idOrName) {
	Serial.print("idOrName:");
	Serial.println(idOrName);

	for (std::vector<Device>::iterator it = clientLookup.begin(); it != clientLookup.end(); ++it) {
		Device device = *it;

		//Serial.print("device.id:");
		//Serial.println(device.id);

		//Serial.print("device.name:");
		//Serial.println(device.name);

		if (device.id.equals(idOrName)) return device.ip;
		else if (device.name.equals(idOrName)) return device.ip;
	}
	return "not_found";
}

void MasterServer::reDirect() {
	String payload = server.arg("plain");

	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(payload);

	String idOrName = "";
	if (json.containsKey("id")) idOrName = json["id"].asString();
	else if (json.containsKey("name")) idOrName = json["name"].asString();

	String ip = getDeviceIPFromIdOrName(idOrName);
	if (ip == "not_found") {
		String reply = "{\"error\":\"device_not_found\"}";
		server.send(HTTP_CODE_BAD_REQUEST, "application/json", reply);
		return;
	}

	HTTPClient http;
	http.begin("http://" + ip + ":" + CLIENT_PORT + server.uri());

	int httpCode = http.sendRequest(getMethod(), payload);

	//HttpCode is only below 0 when there is an issue contacting client
	if (httpCode < 0) {
		http.end();
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

	if (!isValid) server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"id_or_name_field_missing\"}");
	return isValid;
}
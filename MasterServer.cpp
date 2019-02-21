#include "MasterServer.h"

bool MasterServer::start() {
	Serial.println("Entering becomeMaster");

	WiFi.mode(WIFI_STA); //Turns on WIFI_STA for WiFi scanning
	//delay(500);

	//Preps to use static IP
	WiFi.config(STATIC_IP, GATEWAY, SUBNET);

	if (connectToWiFi(WIFI_AP_STA) == false) {
		Serial.println("becomeMaster failed, could not connect");
		return false;
	}

	//Starts up MDNS
	MDNS.begin(masterInfo.hostname);

	//Starts access point for new devices to connect to
	WiFi.softAP(masterInfo.ssid, masterInfo.password);

	addEndpoints();
	addUnknownEndpoint();
	server.begin(MASTER_PORT);
	enableOTAUpdates();
	
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
				String reply = reDirect(HTTP_CODE_OK, device.ip);

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
		else {
			String reply = reDirect(HTTP_CODE_OK);

			if (reply.equals("error") == false) {
				//Sends json from client to caller
				server.send(HTTP_CODE_OK, "application/json", reply);
			}
			else {
				//If cant reach client, reply to caller with error
				server.send(HTTP_CODE_BAD_GATEWAY, "application/json");
			}
		}
	};

	return lambda;
}
std::function<void()> MasterServer::handleMasterSetDeviceName() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleMasterSetDeviceName");

		if (!validId()) return;

		if (isForMe()) handleClientSetName()();
		else {
			String reply = reDirect(HTTP_CODE_OK);

			if (reply.equals("error") == false) {
				//Sends json from client to caller
				server.send(HTTP_CODE_OK, "application/json", reply);
			}
			else {
				//If cant reach client, reply to caller with error
				server.send(HTTP_CODE_BAD_GATEWAY, "application/json");
			}
		}
	};

	return lambda;
}
std::function<void()> MasterServer::handleMasterSetWiFiCreds() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleMasterSetWiFiCreds");

		if (!validId()) return;

		if (isForMe()) handleClientSetWiFiCreds()();
		else {
			String reply = reDirect(HTTP_CODE_OK);

			if (reply.equals("error") == false) {
				//Sends json from client to caller
				server.send(HTTP_CODE_OK, "application/json", reply);
			}
			else {
				//If cant reach client, reply to caller with error
				server.send(HTTP_CODE_BAD_GATEWAY, "application/json");
			}
		}
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
	for (int i = 0; i < devicesFound; ++i) {
		//Call the device's IP and get its info
		String ip = MDNS.IP(i).toString();
		String reply = reDirect(HTTP_CODE_OK);

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
	for (std::vector<Device>::iterator it = clientLookup.begin(); it != clientLookup.end(); ++it) {
		Device device = *it;

		if (device.id.equals(idOrName)) return device.ip;
		else if (device.name.equals(idOrName)) return device.ip;
	}
	return "not_found";
}

String MasterServer::reDirect(t_http_codes expectedCodeReply) {
	String payload = server.arg("plain");

	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(payload);

	String idOrName = "";
	if (json.containsKey("id")) idOrName = json["id"].asString();
	else if (json.containsKey("name")) idOrName = json["name"].asString();

	String ip = getDeviceIPFromIdOrName(idOrName);
	if (ip == "not_found") return "{\"error\":\"id_or_name_not_found\"}";
	else return reDirect(expectedCodeReply, ip);
}

String MasterServer::reDirect(t_http_codes expectedCodeReply, String ip) {
	String path = server.uri();
	Serial.println("path: " + path);

	String url = "http://" + ip + ":80" + path;
	Serial.println("url: " + url);

	HTTPClient http;
	http.begin(url);

	Serial.print("method: ");
	Serial.println(strMethod());

	String payload = "";
	if (server.hasArg("plain")) payload = server.arg("plain");
	Serial.print("payload: ");
	Serial.println(payload);

	String toReturn = "";
	if (http.sendRequest(strMethod(), payload) == expectedCodeReply) {
		toReturn = http.getString();
	}
	else toReturn = "error";

	http.end();
	return toReturn;
}

const char* MasterServer::strMethod() {
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
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(server.arg("plain"));
	if (!json.success()) isValid = false;
	else if (!json.containsKey("id") && !json.containsKey("name")) isValid = false;

	if (!isValid) server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"id_or_name_field_missing\"}");
	return isValid;
}

bool MasterServer::connectToWiFi(WiFiMode wifiMode) {
	connectToWiFi(creds.load(), wifiMode);
}
bool MasterServer::connectToWiFi(WiFiInfo info, WiFiMode wifiMode) {
	Serial.println("Connecting to:");
	Serial.print("SSID: ");
	Serial.println(info.ssid);
	Serial.print("Password: ");
	Serial.println(info.password);
	Serial.print("Name: ");
	Serial.println(info.hostname);
	Serial.print("Id: ");
	Serial.println(ESP.getChipId());

	WiFi.mode(wifiMode);
	WiFi.hostname(info.hostname);
	WiFi.begin(info.ssid, info.password);

	// Wait for connection
	int i = 0;
	while (WiFi.status() != WL_CONNECTED && i < 15) {
		delay(500);
		Serial.print(".");
		i++;
	}

	Serial.println("");

	if (WiFi.status() == WL_CONNECTED) {
		Serial.println("Connected to: ");
		Serial.print("SSID: ");
		Serial.println(info.ssid);
		Serial.print("IP address: ");
		Serial.println(WiFi.localIP().toString());

		return true;
	}
	else {
		Serial.println("Could not connect!");

		return false;
	}
}
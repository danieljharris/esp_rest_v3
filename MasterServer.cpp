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

			if (device.ip.equals(myIp)) devices.add(getDeviceInfo());
			else {
				String reply = reDirect(HTTP_CODE_OK, device.ip);
				if (reply.equals("error") == false) {
					DynamicJsonBuffer jsonBuffer2;
					devices.add(jsonBuffer2.parseObject(reply));
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

		if (!validName()) {
			server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"name_field_missing\"}");
			return;
		}

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

		if (!validName()) {
			server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"name_field_missing\"}");
			return;
		}

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

		if (!validName()) {
			server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"name_field_missing\"}");
			return;
		}

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
	myself.name = info.hostname;
	myself.ip = WiFi.localIP().toString();
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
			String name = input["name"];
			newDevice.ip = ip;
			newDevice.name = name;

			clientLookup.push_back(newDevice);
		}
	}

	Serial.println("Leaving refreshLookup");
}

String MasterServer::getDeviceIPFromName(String name) {
	for (std::vector<Device>::iterator it = clientLookup.begin(); it != clientLookup.end(); ++it) {
		Device device = *it;

		if (device.name.equalsIgnoreCase(name) == true) {
			return device.ip;
		}
	}
	return "not_found";
}

String MasterServer::reDirect(t_http_codes expectedCodeReply) {
	String payload = server.arg("plain");

	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(payload);
	String name = json["name"];

	String ip = getDeviceIPFromName(name);
	if (ip == "not_found") return "{\"error\":\"name_not_found\"}";
	else return reDirect(expectedCodeReply, ip);
}

String MasterServer::reDirect(t_http_codes expectedCodeReply, String ip) {
	//if (ip.length() == 0) return "error";

	String payload = server.arg("plain");

	String path = server.uri();

	Serial.println("path: " + path);

	String url = "http://" + ip + ":80" + path;

	Serial.println("url: " + url);

	HTTPClient http;
	http.begin(url);

	String toReturn;

	Serial.print("strMethod(): ");
	Serial.println(strMethod());
	Serial.println("payload: " + payload);

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
	case HTTP_ANY:     method = "GET";
	case HTTP_GET:     method = "GET";
	case HTTP_POST:    method = "POST";
	case HTTP_PUT:     method = "PUT";
	case HTTP_PATCH:   method = "PATCH";
	case HTTP_DELETE:  method = "DELETE";
	case HTTP_OPTIONS: method = "OPTIONS";
	}

	return method;
}

bool MasterServer::isForMe() {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(server.arg("plain"));
	if (!json.success()) { server.send(HTTP_CODE_BAD_REQUEST); return false; }
	if (!json.containsKey("name")) { server.send(HTTP_CODE_BAD_REQUEST); return false; }

	String name = json["name"];

	WiFiInfo info = creds.load();
	String myName = info.hostname;
	return myName.equals(name);
}

bool MasterServer::validName() {
	if (!server.hasArg("plain")) return false;
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(server.arg("plain"));
	if (!json.success()) return false;
	if (!json.containsKey("name")) return false;

	return true;
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
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

	Serial.println("Starting MDNS...");
	startMDNS();

	Serial.println("Opening soft access point...");
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

void MasterServer::update() {
	MDNS.removeServiceQuery(hMDNSServiceQuery);
	hMDNSServiceQuery = MDNS.installServiceQuery("UNI_FRAME", "tcp", Callback);
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

		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();
		JsonArray& devices = json.createNestedArray("devices");

		devices.add(jsonBuffer.parseObject(getDeviceInfo()));

		for (auto info : MDNS.answerInfo(hMDNSServiceQuery)) {
			if (info.txtAvailable()) {
				JsonObject& device = jsonBuffer.createObject();

				for (auto kv : info.keyValues()) {
					if (String(kv.first).equals("id")) device["id"] = String(kv.second).toInt();
					else if (String(kv.first).equals("ip")) device["ip"] = String(kv.second);
					else if (String(kv.first).equals("name")) device["name"] = String(kv.second);
				}
				devices.add(device);
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

MDNSResponder::MDNSServiceQueryCallbackFunc MasterServer::QueryCallback() {
	MDNSResponder::MDNSServiceQueryCallbackFunc callback = [=](MDNSResponder::MDNSServiceInfo serviceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent) {
		if (answerType == MDNSResponder::AnswerType::Txt) {
			Serial.println("Entering QueryCallback");

			//for (auto kv : serviceInfo.keyValues()) {
			//	Serial.println(String(kv.first) + ":" + String(kv.second));
			//}
		}
	};
	return callback;
}

String MasterServer::getDeviceIPFromIdOrName(String idOrName) {
	Serial.print("idOrName:");
	Serial.println(idOrName);

	for (auto info : MDNS.answerInfo(hMDNSServiceQuery)) {
		if (info.txtAvailable()) {
			String id = "";
			String ip = "";
			String name = "";
			for (auto kv : info.keyValues()) {
				if (String(kv.first).equals("id")) id = String(kv.second);
				else if (String(kv.first).equals("ip")) ip = String(kv.second);
				else if (String(kv.first).equals("name")) name = String(kv.second);
			}
			if (id.equals(idOrName)) return ip;
			else if (name.equals(idOrName)) return ip;
		}
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

void MasterServer::startMDNS() {
	MDNS.begin(MASTER_INFO.hostname); //Starts up MDNS

	hMDNSServiceQuery = MDNS.installServiceQuery("UNI_FRAME", "tcp", Callback);
}
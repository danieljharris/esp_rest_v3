#include "ClientServer.h"

bool ClientServer::start() {
	Serial.println("Entering startClient");

	WiFi.mode(WIFI_STA);

	if (!findAndConnectToMaster()) {
		Serial.println("Failed to become client");
		return false;
	}
	else Serial.println("I am a client");

	//Sets the ESP's output pin to OFF
	pinMode(GPIO_PIN, OUTPUT);
	digitalWrite(GPIO_PIN, HIGH);

	Serial.println("Letting master know I exist...");
	checkinWithMaster();

	Serial.println("Adding endpoints...");
	addEndpoints();

	Serial.println("Adding unknown endpoint...");
	addUnknownEndpoint();

	Serial.println("Starting server...");
	server.begin(CLIENT_PORT);

	Serial.println("Starting MDNS...");
	startMDNS();

	Serial.println("Enableing OTA updates...");
	enableOTAUpdates();

	Serial.println("Ready!");
	return true;
}

void ClientServer::update() { checkinWithMaster(); }

//Client endpoints
void ClientServer::addEndpoints() {
	for (std::vector<Endpoint>::iterator it = clientEndpoints.begin(); it != clientEndpoints.end(); ++it) {
		server.on(it->path, it->method, it->function);
	}
}
std::function<void()> ClientServer::handleClientGetInfo() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleClientGetInfo");
		server.send(HTTP_CODE_OK, "application/json", getDeviceInfo());
	};
	return lambda;
}
std::function<void()> ClientServer::handleClientSetDevice() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleClientSetDevice");

		if (!server.hasArg("plain")) { server.send(HTTP_CODE_BAD_REQUEST); return; }
		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.parseObject(server.arg("plain"));
		if (!json.success()) { server.send(HTTP_CODE_BAD_REQUEST); return; }
		if (!json.containsKey("action")) {
			server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"action_field_missing\"}");
			return;
		}

		bool lastState = gpioPinState;

		if (json["action"] == "toggle") {
			power_toggle();
		}
		else if (json["action"] == "pulse") {
			if (json["direction"] == "off_on_off") {
				power_on();
				delay(1000);
				power_off();
			}
			else if (json["direction"] == "on_off_on") {
				power_off();
				delay(1000);
				power_on();
			}
			else {
				server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"direction_field_missing_or_invalid\"}");
				return;
			}
		}
		else if (json["action"] == "set") {
			if (json["power"] == true) {
				power_on();
			}
			else if (json["power"] == false) {
				power_off();
			}
			else {
				server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"power_field_missing_or_invalid\"}");
				return;
			}
		}
		else {
			server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"action_field_invalid\"}");
			return;
		}

		//Creates return json object
		DynamicJsonBuffer jsonBuffer2;
		JsonObject& output = jsonBuffer2.createObject();

		output["power"] = gpioPinState;
		output["last_state"] = lastState;

		String result;
		output.printTo(result);

		server.send(HTTP_CODE_OK, "application/json", result);
	};
	return lambda;
}
std::function<void()> ClientServer::handleClientSetName() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleClientSetName");

		if (!server.hasArg("plain")) { server.send(HTTP_CODE_BAD_REQUEST); return; }
		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.parseObject(server.arg("plain"));
		if (!json.success()) { server.send(HTTP_CODE_BAD_REQUEST); return; }
		if (!json.containsKey("new_name")) {
			server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"new_name_field_missing\"}");
			return;
		}

		String new_name = json["new_name"].asString();
		creds.save(new_name);
		WiFi.hostname(new_name);

		//Creates the return json object
		DynamicJsonBuffer jsonBuffer2;
		JsonObject& output = jsonBuffer2.createObject();

		output["new_name"] = new_name;

		String outputStr;
		output.printTo(outputStr);

		server.send(HTTP_CODE_OK, "application/json", outputStr);
	};
	return lambda;
}
std::function<void()> ClientServer::handleClientSetWiFiCreds() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleClientSetWiFiCreds");

		if (!server.hasArg("plain")) { server.send(HTTP_CODE_BAD_REQUEST); return;}
		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.parseObject(server.arg("plain"));
		if (!json.success()) { server.send(HTTP_CODE_BAD_REQUEST); return; }
		if (!json.containsKey("ssid") && !json.containsKey("ssid")) {
			server.send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"ssid_and_password_fields_missing\"}");
			return;
		}

		WiFiInfo oldInfo = creds.load();

		String ssid = oldInfo.ssid;
		String password = oldInfo.password;

		if (json.containsKey("ssid")) ssid = json["ssid"].asString();
		if (json.containsKey("password")) password = json["password"].asString();

		creds.save(ssid, password);

		//Creates the return json object
		DynamicJsonBuffer jsonBuffer2;
		JsonObject& output = jsonBuffer2.createObject();

		if (json.containsKey("ssid")) output["ssid"] = json["ssid"];
		if (json.containsKey("password")) output["password"] = json["password"];

		String outputStr;
		output.printTo(outputStr);

		server.send(HTTP_CODE_OK, "application/json", outputStr);
	};
	return lambda;
}

//Client creation
void ClientServer::startMDNS() {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	json["id"] = ESP.getChipId();
	json["name"] = creds.load().hostname;
	String jsonName;
	json.printTo(jsonName);

	MDNS.begin(jsonName.c_str());
	MDNS.addService(CLIENT_MDNS_ID, "tcp", 80); //Broadcasts IP so can be seen by other devices
}
bool ClientServer::findAndConnectToMaster() {
	//Can the master access point be found?
	if (findMaster()) {
		//Can I connect to the master access point?
		if (connectToWiFi(MASTER_INFO)) {
			//Can I get and save WiFi credentials from the master?
			if (getAndSaveMainWiFiInfo()) {
				//Can I connect to the WiFi router using the credentials?
				return connectToWiFi(creds.load());
			}
		}
	}
	return false;
}
bool ClientServer::findMaster() {
	Serial.println("Entering findMaster");

	String lookingFor = MASTER_INFO.ssid;
	int foundNetworks = WiFi.scanNetworks();
	for (int i = 0; i < foundNetworks; i++) {
		String current_ssid = WiFi.SSID(i);
		if (current_ssid.equals(lookingFor)) return true;
	}
	return false;
}
bool ClientServer::getAndSaveMainWiFiInfo() {
	Serial.println("Entering getAndSaveMainWiFiInfo");

	String ip = WiFi.gatewayIP().toString() + ":" + MASTER_PORT;
	String url = "http://" + ip + "/wifi_info";

	HTTPClient http;
	http.begin(url);

	if (http.GET() == HTTP_CODE_OK) {
		String payload = http.getString();
		http.end();

		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.parseObject(payload);
		if (!json.success()) { return false; }
		if (!json.containsKey("ssid") && !json.containsKey("password") && !json.containsKey("master_ip")) return false;

		masterIP = json["master_ip"].asString();
		String ssid = json["ssid"].asString();
		String password = json["password"].asString();

		creds.save(ssid, password);
		return true;
	}
	else {
		http.end();
		Serial.println("Failed to get WiFi credentials from master");
		return false;
	}
}

//Client to master handleing
void ClientServer::checkinWithMaster() {
	Serial.println("Checking up on master");

	HTTPClient http;
	http.begin("http://" + masterIP + ":" + String(MASTER_PORT) + "/checkin");
	int httpCode = http.sendRequest("POST", getDeviceInfo());
	http.end();

	if (httpCode != HTTP_CODE_OK && !updateMasterIP()) {
		electNewMaster();
	}
}
bool ClientServer::updateMasterIP() {
	Serial.println("Entering updateMasterIP");

	if (MDNS.queryService(MASTER_MDNS_ID, "tcp") < 1) return false;
	else {
		masterIP = MDNS.answerIP(0).toString();
		return true;
	}
}
void ClientServer::electNewMaster() {
	Serial.println("Entering electNewMaster");

	// ### Should call all other clients maybe?
	// ### Might have issue with looping?

	//Initalises the chosen id to the id of the current device
	String myHostId = String(ESP.getChipId());
	String chosenHostId = myHostId;

	//Prints details for each service found
	Serial.print("My host id: " + chosenHostId);
	Serial.println("Other host ids: ");

	//Query for client devices
	int devicesFound = MDNS.queryService(CLIENT_MDNS_ID, "tcp");
	for (int i = 0; i < devicesFound; ++i) {
		String reply = MDNS.answerHostname(i);

		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.parseObject(reply);

		if (json.success() && json.containsKey("id")) {
			String currentHostId = json["id"].asString();

			Serial.println("id: " + currentHostId);

			if (currentHostId > chosenHostId) {
				chosenHostId = currentHostId;
			}
		}
	}

	if (myHostId.equals(chosenHostId)) {
		Serial.println("I've been chosen as the new master");
		MDNS.close();
		ESP.restart();
	}
	else {
		Serial.println("I've been chosen to stay as a client");
		delay(1000 * 10); //Waits for 10 seconds for new master to connect
	}
}

//General reusable functions for client & master servers
String ClientServer::getDeviceInfo() {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

	json["id"] = ESP.getChipId();
	json["ip"] = WiFi.localIP().toString();
	json["name"] = creds.load().hostname;

	String result;
	json.printTo(result);

	return result;
}
bool ClientServer::connectToWiFi(WiFiInfo info) {
	if (info.ssid == "" || info.password == "") {
		Serial.println("Invalid WiFi credentials");
		return false;
	}

	Serial.println("Connecting to:");
	Serial.print("SSID: ");
	Serial.println(info.ssid);
	Serial.print("Password: ");
	Serial.println(info.password);
	Serial.print("Name: ");
	Serial.println(info.hostname);

	WiFi.hostname(info.hostname);
	WiFi.begin(info.ssid, info.password);

	// Wait for connection
	for (int i = 0; WiFi.status() != WL_CONNECTED && i < 15; i++) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");

	if (WiFi.status() == WL_CONNECTED) {
		Serial.print("Connected!\nIP address: ");
		Serial.println(WiFi.localIP().toString());
	}
	else Serial.println("Could not connect!");
	return (WiFi.status() == WL_CONNECTED);
}

//Power controls
void ClientServer::power_toggle() {
	Serial.println("Entering power_toggle");

	if (gpioPinState == false) power_on();
	else power_off();
}
void ClientServer::power_on() {
	Serial.println("Entering power_on");

	gpioPinState = true;
	digitalWrite(GPIO_PIN, LOW);
}
void ClientServer::power_off() {
	Serial.println("Entering power_off");

	gpioPinState = false;
	digitalWrite(GPIO_PIN, HIGH);
}
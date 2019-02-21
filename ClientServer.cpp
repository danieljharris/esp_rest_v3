#include "ClientServer.h"

bool ClientServer::start() {
	Serial.println("Entering startClient");

	//Starts up MDNS
	char hostString[16] = { 0 };
	strcat(hostString, getDeviceHostName());
	MDNS.begin(hostString);

	//Serial.print("hostString: ");
	//Serial.println(hostString);

	//Broadcasts IP so can be seen by other devices
	MDNS.addService(MDNS_ID, "tcp", 80);

	Serial.println("Enableing OTA updates...");
	enableOTAUpdates();

	Serial.println("Adding endpoints...");
	addEndpoints();

	Serial.println("Adding unknown endpoint...");
	addUnknownEndpoint();

	Serial.println("Starting server...");
	server.begin(80);
}

void ClientServer::addEndpoints() {
	std::function<void()> clientGetInfo = handleClientGetInfo();
	std::function<void()> clientSetDevice = handleClientSetDevice();
	std::function<void()> clientSetName = handleClientSetName();
	std::function<void()> clientSetWiFiCreds = handleClientSetWiFiCreds();

	server.on("/device", HTTP_GET, clientGetInfo);
	server.on("/device", HTTP_POST, clientSetDevice);
	server.on("/name", HTTP_POST, clientSetName);
	server.on("/credentials", HTTP_POST, clientSetWiFiCreds);
}

bool ClientServer::electNewMaster() {
	Serial.print("Entering electNewMaster");
	//Initalises the chosen host name to the host name of the current device
	String myHostName = getDeviceHostName();

	String chosenHostName = myHostName;

	//Prints details for each service found
	Serial.print("My host name: ");
	Serial.print(chosenHostName);
	Serial.print("Other host names: ");

	//Send out query for ESP_REST devices
	int devicesFound = MDNS.queryService(MDNS_ID, "tcp");
	for (int i = 0; i < devicesFound; ++i) {
		String currentHostName = MDNS.hostname(i);
		if (currentHostName > chosenHostName) {
			chosenHostName = currentHostName;
		}
		Serial.print(currentHostName);
	}

	return myHostName.equals(chosenHostName);
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

		creds.save(json["new_name"].asString());

		//Creates the return json object
		DynamicJsonBuffer jsonBuffer2;
		JsonObject& output = jsonBuffer2.createObject();

		output["new_name"] = json["new_name"];

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

String ClientServer::getDeviceInfo() {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

	WiFiInfo info = creds.load();
	json["name"] = info.hostname;
	json["ip"] = WiFi.localIP().toString();
	json["powered"] = gpioPinState;

	String result;
	json.printTo(result);

	return result;
}

//Power control
void ClientServer::power_toggle() {
	Serial.println("Entering power_toggle");

	if (gpioPinState == false) power_on();
	else power_off();
}

void ClientServer::power_on() {
	Serial.println("Entering power_on");

	gpioPinState = true;
	digitalWrite(gpioPin, LOW);
}

void ClientServer::power_off() {
	Serial.println("Entering power_off");

	gpioPinState = false;
	digitalWrite(gpioPin, HIGH);
}
#include "Client.h"

bool Client::start() {
	Serial.println("Entering startClient");

	//Starts up MDNS
	char hostString[16] = { 0 };
	sprintf(hostString, "ESP_%06X", ESP.getChipId());
	MDNS.begin(hostString);

	//Broadcasts IP so can be seen by master
	MDNS.addService(MDNS_ID, "tcp", 80);

	//Client server setup
	server.on("/device", HTTP_GET, this->handleGetInfo);
	server.on("/device", HTTP_POST, this->handleSetState);
	server.on("/name", HTTP_POST, this->handleSetName);

	server.onNotFound(handleUnknown);
	server.begin();
}

bool Client::getAndSaveMainWiFiInfo() {
	Serial.println("Entering getAndSaveMainWiFiInfo");

	String ip = WiFi.gatewayIP().toString() + ":" + MASTER_PORT;
	String url = "http://" + ip + "/wifi_info";

	HTTPClient http;
	http.begin(url);

	if (http.GET() == HTTP_CODE_OK) {
		String payload = http.getString();

		DynamicJsonBuffer jsonBuffer;
		JsonObject& input = jsonBuffer.parseObject(payload);
		String ssid = input["ssid"];
		String password = input["password"];

		new Persistent_Store->saveWiFiCredentials(ssid, password);

		http.end();
		return true;
	}
	else {
		http.end();
		return false;
	}
}

void Client::handleGetInfo() {
	Serial.println("Entering handleGetInfo");

	DynamicJsonBuffer jsonBuffer;
	JsonObject& output = jsonBuffer.createObject();

	output["name"] = getName();
	output["powered"] = getState();
	output["ip"] = WiFi.localIP().toString();

	String result;
	output.printTo(result);

	server.send(HTTP_CODE_OK, "application/json", result);
}

void Client::handleSetState() {
	Serial.println("Entering handleSetState");

	if (server.hasArg("plain") == false)
	{
		server.send(HTTP_CODE_BAD_REQUEST);
		return;
	}

	DynamicJsonBuffer jsonBuffer;
	JsonObject& input = jsonBuffer.parseObject(server.arg("plain"));

	bool lastState = gpioPinState;

	if (input["action"] == "toggle") {
		switch (getState()) {
		case true: setState(false);
		case false: setState(true);
		}
	}
	else if (input["action"] == "pulse") {
		setState(true);
		delay(1000);
		setState(false);
	}
	else if (input["action"] == "set") {
		setState(input["power"]);
	}

	JsonObject& output = jsonBuffer.createObject();

	output["power"] = gpioPinState;
	output["last_state"] = lastState;

	String result;
	output.printTo(result);
	server.send(HTTP_CODE_OK, "application/json", result);
}

void Client::handleSetName() {
	Serial.println("Entering handleSetName");

	if (server.hasArg("plain") == false)
	{
		server.send(HTTP_CODE_BAD_REQUEST);
		return;
	}

	DynamicJsonBuffer jsonBuffer;
	JsonObject& input = jsonBuffer.parseObject(server.arg("plain"));
	String newName = input["new_name"];

	setName(newName);

	server.send(HTTP_CODE_OK, "application/json");
}

string Client::getName()
{
	WiFiInfo into = new Persistent_Store->loadWiFiCredentials();
	return into.name;
}

bool Client::getState()
{
	return poweredState;
}

void Client::setName(String newName)
{
	new Persistent_Store->saveWiFiCredentials(newName);
}

void Client::setState(bool newState)
{
	gpioPinState = newState;

	switch (newState) {
	case true: digitalWrite(gpioPin, LOW);
	case false: digitalWrite(gpioPin, HIGH);
	}
}
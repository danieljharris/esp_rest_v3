#include "Client_Mode.h"

bool Client_Mode::start() {
	Serial.println("Entering startClient_Mode");

	//Starts up MDNS
	char hostString[16] = { 0 };
	sprintf(hostString, "ESP_%06X", ESP.getChipId());
	MDNS.begin(hostString);

	//Broadcasts IP so can be seen by master
	MDNS.addService(MDNS_ID, "tcp", 80);

	//Client_Mode server setup
	server.on("/device", HTTP_GET, this->handleGetInfo);
	server.on("/device", HTTP_POST, this->handleSetState);
	server.on("/name", HTTP_POST, this->handleSetName);

	server.onNotFound(this->handleUnknown);
	server.begin();
}

void Client_Mode::handleGetInfo() {
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

void Client_Mode::handleSetState() {
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

void Client_Mode::handleSetName() {
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

string Client_Mode::getName()
{
	WiFiInfo into = new Persistent_Store->loadWiFiCredentials();
	return into.name;
}

bool Client_Mode::getState()
{
	return poweredState;
}

void Client_Mode::setName(String newName)
{
	new Persistent_Store->saveWiFiCredentials(newName);
}

void Client_Mode::setState(bool newState)
{
	gpioPinState = newState;

	switch (newState) {
	case true: digitalWrite(gpioPin, LOW);
	case false: digitalWrite(gpioPin, HIGH);
	}
}
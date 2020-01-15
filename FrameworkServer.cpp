//FrameworkServer.cpp

#include "FrameworkServer.h"

FrameworkServer::~FrameworkServer() { stop(); }

void FrameworkServer::stop() {
	Serial.println("Entering stop");

	server.close();
	server.stop();
}

char* FrameworkServer::getDeviceHostName() {
	char* newName = "ESP_";
	char hostName[10];
	sprintf(hostName, "%d", ESP.getChipId());
	strcat(newName, hostName);

	return newName;
}

void FrameworkServer::enableOTAUpdates() {
	//Enabled "Over The Air" updates so that the ESPs can be updated remotely 
	ArduinoOTA.setPort(8266);
	ArduinoOTA.setHostname(getDeviceHostName());

	ArduinoOTA.onStart([]() {
		MDNS.close(); //Close to not interrupt other devices
		Serial.println("OTA Update Started...");
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nOTA Update Ended!");
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
	});

	ArduinoOTA.begin();
}

void FrameworkServer::addUnknownEndpoint() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleClientUnknown");

		Serial.println("");
		Serial.print("Unknown command: ");
		Serial.println(server.uri());

		Serial.print("Method: ");
		Serial.println(server.method());

		server.send(HTTP_CODE_NOT_FOUND);
	};
	server.onNotFound(lambda);
}

void FrameworkServer::addEndpoints(std::vector<Endpoint> endpoints) {
	for (std::vector<Endpoint>::iterator it = endpoints.begin(); it != endpoints.end(); ++it) {
		server.on(it->path, it->method, it->function);
	}
}

void FrameworkServer::relayMode(uint8_t type)
{
	for (int i = 0; i < 8; i++) {
		int pin = RELAYS[i];
		pinMode(pin, type);
		digitalWrite(pin, HIGH);
	}
}

void FrameworkServer::relayWrite(uint8_t pin, bool state)
{
	int adjusted_pin = pin - 1;
	int relay_pin = RELAYS[adjusted_pin];
	digitalWrite(relay_pin, state);
}

void FrameworkServer::relayToggle(uint8_t pin)
{
	int adjusted_pin = pin - 1;

	int relay_pin = RELAYS[adjusted_pin];
	bool currentState = RELAYS_STATE[adjusted_pin];

	bool toggledState = !currentState;
	RELAYS_STATE[adjusted_pin] = toggledState;

	digitalWrite(relay_pin, toggledState);
}

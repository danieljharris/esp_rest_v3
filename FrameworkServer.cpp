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

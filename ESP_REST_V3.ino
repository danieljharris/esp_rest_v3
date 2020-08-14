//Main.ino

#include "ConnectedDevice.h"
#include "FrameworkServer.h"
#include "Setupserver.h"
#include "ClientServer.h"
#include "MasterServer.h"

#include <ArduinoOTA.h>
#include <PolledTimeout.h>

FrameworkServer* server = new FrameworkServer;

//The frequency to call server update (1000 = 1 second)
esp8266::polledTimeout::periodic period(1000 * 10);

void setup() {
	Serial.begin(19200);
	Serial.println("\nStarting ESP8266...");

	server = new ClientServer();
	if (!server->start()){
		server = new MasterServer();
		if (!server->start()) {
			server = new SetupServer();
			if (!server->start()) {
				Serial.println("\nFailed to become client, master and setup. Restarting...");
				ESP.restart();
			}
		}
	}
}

void loop() {
	server->handle();
	ArduinoOTA.handle();

	if (period) server->update();
}
#include "FrameworkServer.h"
#include "Setupserver.h"
#include "ClientServer.h"
#include "MasterServer.h"

#include <ArduinoOTA.h>
#include <PolledTimeout.h>

/*
	TODO:
	() Add apidoc for endpoints
	-() Look into implementing SmartConfig to connect devices to router
	-() Dont run turn on function if already on, and dont run turn off function if already off
	DONE:
	() Find a way of refreshing clientLookup without removing master from it (No need to remove master)
	() Add functionality for custom circuit board and input detection (Like PC's ESP)
	() Try to move some of the functions to seperate files (Get practice with hearder files)
	() Try to reduce the sleep timers to make things faster
	() Allow Master endpoints to be called using device IDs as well as Names (Will fix the issue of 2 devices having the same name)
	() Add functionality to make clients automaticly become master if master gets disconnected
	() If ESP does not reply when Master is doing get devices, then remove that ESP from connectedClients
	() Add master to its own connectedClients or master can not be accessed
	() Be able to change the name of the master ESP (Maybe see if the client server can be run at the same time as master?)
	() Populate clientLookup in becomeMaster
	() Restructure master endpoints to check IP first and not during the sending
	() Add endpoint that updates clientLookup without replying to user (If that is faster, if not dont bother)
	() Make sure all included libraries are needed/used
	() Make SSID a drop down menu in config site
*/

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
			server = new SetupServer(); // ### Should keep checking for master here?
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


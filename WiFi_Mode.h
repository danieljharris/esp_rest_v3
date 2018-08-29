#pragma once

#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include "http_execute.h"
#include "Persistent_Store.h"

enum modes {
	Unknown = 0,
	Config = 1,
	Client = 2,
	Master = 3
};

class WiFi_Mode
{
protected:
	modes currentMode = Unknown;

	ESP8266WebServer server;

	void handleUnknown() {
		String UnknownUri = server.uri();
		Serial.print("Unknown command: ");
		Serial.println(UnknownUri);
		server.send(404);
	};

	void(*resetFunc) (void) = 0; //declare reset function at address 0

public:
	virtual bool start() { return NULL; };
	void stop() { server.client().stop(); };

	virtual void checkIn() { };

	modes getMode() { return currentMode; };
	void handle() { server.handleClient(); };
};
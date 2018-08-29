#pragma once

#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include "Persistent_Store.h"

class WiFi_Mode
{
protected:
	ESP8266WebServer server;

	void handleUnknown() {
		String UnknownUri = server.uri();
		Serial.print("Unknown command: ");
		Serial.println(UnknownUri);
		server.send(404);
	};

public:
	virtual bool start() { return NULL; };
	virtual bool stop()  { return NULL; };
};
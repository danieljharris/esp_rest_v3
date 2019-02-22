// FrameworkServer.h

#ifndef _FRAMEWORKSERVER_h
#define _FRAMEWORKSERVER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "WiFiCredentials.h"
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>

class FrameworkServer {
protected:
	ESP8266WebServer server;
	WiFiCredentials creds;

	char* getDeviceHostName();
	void enableOTAUpdates();
	void addUnknownEndpoint();
	virtual void addEndpoints() {};

public:
	virtual bool start() {};
	void handle() { server.handleClient(); };
	~FrameworkServer();
	void stop();
};

#endif


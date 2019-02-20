// FrameworkServer.h

#ifndef _FRAMEWORKSERVER_h
#define _FRAMEWORKSERVER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
#include "WiFiCredentials.h"

class FrameworkServer {
protected:
	ESP8266WebServer server;
	void(*reset) (void) = 0; //This function restarts the ESP

	WiFiCredentials creds;

	char* getDeviceHostName();
	void enableOTAUpdates();
	virtual void addEndpoints() {};
	void addUnknownEndpoint();

public:
	virtual bool start() {};
	void handle() { server.handleClient(); };
	~FrameworkServer();
	void stop();
};

#endif


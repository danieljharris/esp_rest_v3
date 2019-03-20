// FrameworkServer.h

#ifndef _FRAMEWORKSERVER_h
#define _FRAMEWORKSERVER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "WiFiCredentials.h"
#include "Config.h"
#include "Endpoint.h"

#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

class FrameworkServer {
protected:
	ESP8266WebServer server;
	WiFiCredentials creds;

	char* getDeviceHostName();
	void enableOTAUpdates();
	void addUnknownEndpoint();
	void addEndpoints(std::vector<Endpoint> endpoints);

public:
	virtual bool start() {};
	virtual void update() {};
	void handle() { server.handleClient(); };
	~FrameworkServer();
	void stop();
};

#endif


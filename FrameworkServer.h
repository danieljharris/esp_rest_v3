// FrameworkServer.h

#ifndef _FRAMEWORKSERVER_h
#define _FRAMEWORKSERVER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "WiFiCredentials.h"
#include "ConnectedDevice.h"
#include "Config.h"
#include "Endpoint.h"

#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

class FrameworkServer {
protected:
	ESP8266WebServer server;
	WiFiCredentials creds;
	ConnectedDevice connected;

	char* getDeviceHostName();
	void enableOTAUpdates();
	void addUnknownEndpoint();
	void addEndpoints(std::vector<Endpoint> endpoints);
	void selfRegister();
	void configUpdate();

public:
	virtual bool start() {};
	virtual void update() {};
	virtual void handle() { server.handleClient(); };
	~FrameworkServer();
	void stop();
};

#endif


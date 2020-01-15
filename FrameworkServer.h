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

#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

class FrameworkServer {
protected:
	const int RELAYS[8] = { 13,12,14,2,0,4,5,16 };
	bool RELAYS_STATE[8] = { true, true, true, true, true, true, true, true };

	ESP8266WebServer server;
	WiFiCredentials creds;

	char* getDeviceHostName();
	void enableOTAUpdates();
	void addUnknownEndpoint();
	void addEndpoints(std::vector<Endpoint> endpoints);

	//To handle relay
	void relayMode(uint8_t type);
	void relayWrite(uint8_t pin, bool state);
	void relayToggle(uint8_t pin);

public:
	virtual bool start() {};
	virtual void update() {};
	virtual void handle() { server.handleClient(); };
	~FrameworkServer();
	void stop();
};

#endif


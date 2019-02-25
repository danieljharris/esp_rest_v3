// ClientServer.h

#ifndef _CLIENTSERVER_h
#define _CLIENTSERVER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "Config.h"
#include "FrameworkServer.h"
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <vector>

typedef struct Endpoint {
	String path;
	HTTPMethod method;
	std::function<void()> function;

	Endpoint(String path, HTTPMethod method, std::function<void()> function) {
		this->path = path;
		this->method = method;
		this->function = function;
	}
};

class ClientServer : public FrameworkServer {
private:
	void addEndpoints();

	std::function<void()> handleClientGetInfo();
	std::function<void()> handleClientSetDevice();
	std::function<void()> handleClientSetName();
	std::function<void()> handleClientSetWiFiCreds();

	String masterIP = "";

	void startMDNS();
	void checkinWithMaster();

	void electNewMaster();

	bool findAndConnectToMaster();
	bool findMaster();
	bool getAndSaveMainWiFiInfo();

	//Power control
	void power_toggle();
	void power_on();
	void power_off();
	bool gpioPinState = false;

protected:
	std::vector<Endpoint> clientEndpoints{
		Endpoint("/device", HTTP_GET, handleClientGetInfo()),
		Endpoint("/device", HTTP_POST, handleClientSetDevice()),
		Endpoint("/name", HTTP_POST, handleClientSetName()),
		Endpoint("/credentials", HTTP_POST, handleClientSetWiFiCreds())
	};

	String getDeviceInfo();
	bool connectToWiFi(WiFiInfo info);

public:
	bool start();
	void update();
};

#endif


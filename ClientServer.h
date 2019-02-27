// ClientServer.h

#ifndef _CLIENTSERVER_h
#define _CLIENTSERVER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "FrameworkServer.h"

#include <ESP8266mDNS.h>
#include <ArduinoJson.h>

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
	//Client endpoint handleing
	void addEndpoints();
	std::function<void()> handleClientGetInfo();
	std::function<void()> handleClientSetDevice();
	std::function<void()> handleClientSetName();
	std::function<void()> handleClientSetWiFiCreds();
	std::function<void()> handleClientRestart();
	std::function<void()> handleClientNewMaster();

	//Client creation
	void startMDNS();
	bool findAndConnectToMaster();
	bool findMaster();
	bool getAndSaveMainWiFiInfo();

	//Client to master handleing
	String masterIP = "";
	void checkinWithMaster();
	bool updateMasterIP();
	void electNewMaster();
	void becomeMaster(std::vector<String> clientIPs);

	//Power control
	void power_toggle();
	void power_on();
	void power_off();
	bool gpioPinState = false;

protected:
	//Client endpoints
	std::vector<Endpoint> clientEndpoints{
		Endpoint("/device", HTTP_GET, handleClientGetInfo()),
		Endpoint("/device", HTTP_POST, handleClientSetDevice()),
		Endpoint("/name", HTTP_POST, handleClientSetName()),
		Endpoint("/credentials", HTTP_POST, handleClientSetWiFiCreds()),
		Endpoint("/restart", HTTP_POST, handleClientRestart()),
		Endpoint("/master", HTTP_POST, handleClientNewMaster())
	};

	//General reusable functions for client & master servers
	String getDeviceInfo();
	bool connectToWiFi(WiFiInfo info);

public:
	bool start();
	void update();
};

#endif


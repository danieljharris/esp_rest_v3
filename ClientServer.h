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

class ClientServer : public FrameworkServer {
private:
	void addEndpoints();

	void startMDNS();

	bool electNewMaster();
	bool getAndSaveMainWiFiInfo();
	bool findAndConnectToMaster();
	bool findMaster();

	//Power control
	void power_toggle();
	void power_on();
	void power_off();
	bool gpioPinState = false;

protected:
	std::function<void()> handleClientGetInfo();
	std::function<void()> handleClientSetDevice();
	std::function<void()> handleClientSetName();
	std::function<void()> handleClientSetWiFiCreds();

	String getDeviceInfo();
	bool connectToWiFi(WiFiInfo info);

public:
	bool start();
};

#endif


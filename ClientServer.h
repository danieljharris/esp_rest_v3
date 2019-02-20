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

class ClientServer : public FrameworkServer {
private:
	void addEndpoints();
	bool electNewMaster();

	int gpioPin = 0;
	bool gpioPinState = false;

	//Power control
	void power_toggle();
	void power_on();
	void power_off();

protected:
	std::function<void()> handleClientGetInfo();
	std::function<void()> handleClientSetDevice();
	std::function<void()> handleClientSetName();
	std::function<void()> handleClientSetWiFiCreds();

	JsonObject& getDeviceInfo();

	const String MDNS_ID = "ESP_NETWORK";

public:
	bool start();
};

#endif


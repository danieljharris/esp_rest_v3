// ClientServer.h

#ifndef _CLIENTSERVER_h
#define _CLIENTSERVER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "FrameworkServer.h"

#include <ArduinoJson.h>

class ClientServer : public FrameworkServer {
private:
	//Client endpoint handleing
	std::function<void()> handleClientGetInfo();
	std::function<void()> handleClientPostDevice();
	std::function<void()> handleClientPostName();
	std::function<void()> handleClientPostWiFiCreds();
	std::function<void()> handleClientPostRestart();
	std::function<void()> handleClientPostNewMaster();

	//Client creation
	void startMDNS();
	bool findAndConnectToMaster();
	bool findMaster();
	bool getAndSaveMainWiFiInfo();

	//Client to master transition handleing
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

	//Light switch example
	std::function<void()> handleClientPostLightSwitch();

protected:
	//Client endpoints
	std::vector<Endpoint> clientEndpoints{
		Endpoint("/device", HTTP_GET, handleClientGetInfo()),
		Endpoint("/device", HTTP_POST, handleClientPostDevice()),
		Endpoint("/name", HTTP_POST, handleClientPostName()),
		Endpoint("/credentials", HTTP_POST, handleClientPostWiFiCreds()),
		Endpoint("/restart", HTTP_POST, handleClientPostRestart()),
		Endpoint("/master", HTTP_POST, handleClientPostNewMaster()),

		//Light switch example
		Endpoint("/light/switch", HTTP_POST, handleClientPostLightSwitch()),
	};

	//General reusable functions for client & master servers
	String getDeviceInfo();
	bool connectToWiFi(WiFiInfo info);

public:
	bool start();
	void update();
};

#endif


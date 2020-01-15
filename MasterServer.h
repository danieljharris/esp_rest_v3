// MasterServer.h

#ifndef _MASTERSERVER_h
#define _MASTERSERVER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "ClientServer.h"
#include "Device.h"

#include <ESP8266HTTPClient.h>
#include <unordered_set>

#include <Wire.h>
#include <Adafruit_MCP23017.h>

//Used by unordered_set (clientLookup) to make a hash of each device
namespace std {
	template<> struct hash<Device> {
		size_t operator()(const Device& device) const {
			return hash<int>()(device.id.toInt());
		}
	};
};

class MasterServer : public ClientServer {
private:

	Adafruit_MCP23017 mcp;


	//Master endpoint handleing
	void addUnknownEndpoint();
	std::function<void()> handleMasterGetWiFiInfo();
	std::function<void()> handleMasterGetDevices();
	std::function<void()> handleMasterPostCheckin();

	//Master endpoints
	std::vector<Endpoint> masterEndpoints{
		Endpoint("/wifi_info", HTTP_GET, handleMasterGetWiFiInfo()),
		Endpoint("/devices", HTTP_GET, handleMasterGetDevices()),
		Endpoint("/checkin", HTTP_POST, handleMasterPostCheckin()),

		//Light switch example
		//Endpoint("/lights", HTTP_POST, handleMasterPostLightSwitch()),
	};

	//Master creation
	void startMDNS();
	void checkForOtherMasters();
	void closeOtherMasters(std::vector<String> clientIPs);

	//REST request routing
	std::unordered_set<Device> clientLookup;
	String getDeviceIPFromIdOrName(String idOrName);
	void reDirect();
	String reDirect(String ip);
	void expireClientLookup();

	//Helper functions for REST routing
	const char* getMethod();
	bool isForMe();
	bool validIdOrName();

	//Light switch example
	std::function<void()> handleMasterPostLightSwitch();

	//Poster demo
	bool lastInputValue = false;
	void checkInputChange();

public:
	bool start();
	void update();
	void handle();
};

#endif


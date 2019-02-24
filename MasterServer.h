// MasterServer.h

#ifndef _MASTERSERVER_h
#define _MASTERSERVER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "ClientServer.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>

typedef struct Device {
	String id = "Unknown";
	String ip = "Unknown";
	String name = "Unknown";

	Device() {};
	Device(String id, String ip, String name) {
		this->id = id;
		this->ip = ip;
		this->name = name;
	}
};

class MasterServer : public ClientServer {
private:
	void addEndpoints();
	void addUnknownEndpoint();

	std::function<void()> handleMasterGetWiFiInfo();
	std::function<void()> handleMasterGetDevices();
	std::function<void()> handleMasterCheckin();

	std::vector<Endpoint> masterEndpoints{
	Endpoint("/wifi_info", HTTP_GET, handleMasterGetWiFiInfo()),
	Endpoint("/devices", HTTP_GET, handleMasterGetDevices()),
	Endpoint("/checkin", HTTP_POST, handleMasterCheckin()),
	};

	String getDeviceIPFromIdOrName(String idOrName);
	void reDirect();
	String reDirect(String ip);
	const char* getMethod();
	bool isForMe();
	bool validId();

	void startMDNS();
	void refreshLookup();
	std::vector<Device> clientLookup;

public:
	bool start();
	void update();

	//MasterServer() {
	//};
};

#endif


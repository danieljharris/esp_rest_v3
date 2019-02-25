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
#include <unordered_set>
#include <PolledTimeout.h>

using esp8266::polledTimeout::oneShot;

typedef struct Device {
	String id = "Unknown";
	String ip = "Unknown";
	String name = "Unknown";

	oneShot *timeout = new oneShot(0);

	Device() {
		*timeout = oneShot(1000 * 12); //1000 = 1 second
	};
	Device(String id, String ip, String name) {
		this->id = id;
		this->ip = ip;
		this->name = name;
		*timeout = oneShot(1000 * 12); //1000 = 1 second
	}

	//Used by unordered_set (clientLookup) to compare devices
	bool operator==(const Device& device) const {
		if (id == device.id) return true;
		else return false;
	}
};

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
	//Master endpoint handleing
	void addEndpoints();
	void addUnknownEndpoint();
	std::function<void()> handleMasterGetWiFiInfo();
	std::function<void()> handleMasterGetDevices();
	std::function<void()> handleMasterCheckin();

	//Client endpoints
	std::vector<Endpoint> masterEndpoints{
	Endpoint("/wifi_info", HTTP_GET, handleMasterGetWiFiInfo()),
	Endpoint("/devices", HTTP_GET, handleMasterGetDevices()),
	Endpoint("/checkin", HTTP_POST, handleMasterCheckin()),
	};

	//Master creation
	void startMDNS();

	//REST request routing
	std::unordered_set<Device> clientLookup;
	String getDeviceIPFromIdOrName(String idOrName);
	void reDirect();
	String reDirect(String ip);
	void expireClientLookup();

	//Helper functions for REST routing
	const char* getMethod();
	bool isForMe();
	bool validId();

public:
	bool start();
	void update();
};

#endif


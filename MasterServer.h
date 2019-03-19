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
private:
	int timeoutSeconds = 12;
public:
	String id = "Unknown";
	String ip = "Unknown";
	String name = "Unknown";

	//Used to expire devices when they don't checkin
	//Each device must checkin once ever 12 seconds
	oneShot *timeout = new oneShot(0);

	Device() {
		*timeout = oneShot(1000 * timeoutSeconds);
	};
	Device(String id, String ip, String name) {
		this->id = id;
		this->ip = ip;
		this->name = name;
		*timeout = oneShot(1000 * timeoutSeconds);
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
	std::function<void()> handleMasterPostCheckin();

	//Master endpoints
	std::vector<Endpoint> masterEndpoints{
		Endpoint("/wifi_info", HTTP_GET, handleMasterGetWiFiInfo()),
		Endpoint("/devices", HTTP_GET, handleMasterGetDevices()),
		Endpoint("/checkin", HTTP_POST, handleMasterPostCheckin()),

		//Light switch example
		Endpoint("/light/switch", HTTP_POST, handleMasterPostLightSwitch())
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

public:
	bool start();
	void update();
};

#endif


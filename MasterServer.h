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
#include <IPAddress.h>
#include <ArduinoJson.h>

typedef struct Device {
	String ip = "";
	String name = "Unknown";
};

class MasterServer : public ClientServer {
private:
	IPAddress STATIC_IP;
	IPAddress GATEWAY;
	IPAddress SUBNET;

	int MASTER_PORT = 235;
	WiFiInfo masterInfo;

	std::vector<Device> clientLookup;

	std::function<void()> handleMasterGetWiFiInfo();
	std::function<void()> handleMasterGetDevices();
	std::function<void()> handleMasterSetDevice();
	std::function<void()> handleMasterSetDeviceName();
	std::function<void()> handleMasterSetWiFiCreds();
	void addEndpoints();

	void refreshLookup();

	String getDeviceIPFromName(String name);
	String reDirect(t_http_codes expectedCodeReply);
	String reDirect(t_http_codes expectedCodeReply, String ip);
	const char* strMethod();
	bool isForMe();

	bool validName();

	bool connectToWiFi(WiFiMode wifiMode);
	bool connectToWiFi(WiFiInfo info, WiFiMode wifiMode);

public:
	bool start();

	MasterServer() {
		//Config for when master connects to WiFi
		STATIC_IP = IPAddress(192, 168, 1, 100);
		GATEWAY = IPAddress(192, 168, 1, 254);
		SUBNET = IPAddress(255, 255, 255, 0);

		masterInfo = WiFiInfo("ESP-REST-MASTER", "7kAtZZm9ak", "esp8266");
	};
};

#endif


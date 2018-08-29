#pragma once

#include "Persistent_Store.h"
#include "http_execute.h"
#include "WiFi_Mode.h"
#include "Config_Mode.h"
#include "Client_Mode.h"
#include "Master_Mode.h"

class ESP_Device
{
protected:
	WiFi_Mode *currentMode;

	//Config for when master connects to WiFi
	IPAddress *STATIC_IP = new IPAddress(192, 168, 1, 100);
	IPAddress *GATEWAY = new IPAddress(192, 168, 1, 254);
	IPAddress *SUBNET = new IPAddress(255, 255, 255, 0);

	const char* MASTER_SSID = "ESP-REST-MASTER";
	const char* MASTER_PASS = "7kAtZZm9ak";
	const char* MASTER_NAME = "esp8266";
	WiFiInfo masterInfo;
	const int MASTER_PORT = 235;

	int lookupCountdownMax = 100000;
	int lookupCountdown = lookupCountdownMax;

	bool swapMode(WiFi_Mode *newMode);

	bool getAndSaveMainWiFiInfo();

	bool connectToWiFi(WiFiInfo info, WiFiMode wifiMode);
	bool lookForMaster();

	bool rememberWiFiSettings();
	void enableOTAUpdates();

	void(*resetFunc) (void) = 0; //declare reset function at address 0

public:
	ESP_Device();

	void startDevice();
	void handle();
};


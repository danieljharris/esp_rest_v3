#pragma once

#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include "WiFi_Mode.h"
#include "http_execute.h"

class Client : private WiFi_Mode
{
private:
	bool poweredState = false;

	const String MDNS_ID = "ESP_NETWORK";

	void handleGetInfo();
	void handleSetState();
	void handleSetName();

	string getName();
	bool getState();
	void setName(String newName);
	void setState(bool newState);

public:
	bool start();

	bool getAndSaveMainWiFiInfo();
};


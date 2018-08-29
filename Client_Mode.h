#pragma once

#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include "WiFi_Mode.h"

class Client_Mode : public WiFi_Mode
{
protected:
	modes currentMode = Client;

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
};

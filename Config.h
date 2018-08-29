#pragma once

#include "WiFi_Mode.h"

class Config : public WiFi_Mode
{
private:
	const char* SETUP_SSID = "ESP-REST-V3";
	const char* SETUP_PASSWORD = "zJ2f5xSX";

	void handleConfig();
	void handleConnect();

	String makeOptionsSSID();
public:
	bool start();
};


#pragma once
class ESP_Device
{
private:
	const char* MASTER_SSID = "ESP-REST-MASTER";
	const char* MASTER_PASS = "7kAtZZm9ak";
	const char* MASTER_NAME = "esp8266";
	WiFiInfo masterInfo;

public:
	ESP_Device();
	~ESP_Device();

	bool connectToWiFi(WiFiInfo info, WiFiMode wifiMode);
	bool lookForMaster();
};


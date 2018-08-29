#pragma once

#include <EEPROM.h>

using namespace std;

typedef struct WiFiInfo {
	char ssid[32] = "";
	char password[32] = "";
	char name[32] = "Unknown";
};

class Persistent_Store
{
public:

	void saveWiFiCredentials(String nameStr);

	void saveWiFiCredentials(String ssidStr, String passwordStr);
	void saveWiFiCredentials(String ssidStr, String passwordStr, String nameStr);

	WiFiInfo loadWiFiCredentials();
};


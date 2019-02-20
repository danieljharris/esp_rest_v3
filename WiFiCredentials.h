// WiFiCredentials.h

#ifndef _WIFICREDENTIALS_h
#define _WIFICREDENTIALS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

typedef struct WiFiInfo {
	char ssid[32] = { '\0' };
	char password[32] = { '\0' };
	char hostname[32] = { 'U','n','k','n','o','w','n','\0' };

public:
	WiFiInfo() {};
	WiFiInfo(char ssid[32], char password[32], char hostname[32]) {
		strcpy(this->ssid, ssid);
		strcpy(this->password, password);
		strcpy(this->hostname, hostname);
	}
};

class WiFiCredentials {
public:
	void save(String hostname);
	void save(String ssid, String password);
	void save(String ssid, String password, String hostname);
	WiFiInfo load();
};

#endif


#include "Persistent_Store.h"

void Persistent_Store::saveWiFiCredentials(String nameStr) {
	WiFiInfo oldInfo = loadWiFiCredentials();
	String ssidStr = oldInfo.ssid;
	String passwordStr = oldInfo.password;
	saveWiFiCredentials(ssidStr, passwordStr, nameStr);
}

void Persistent_Store::saveWiFiCredentials(String ssidStr, String passwordStr) {
	WiFiInfo oldInfo = loadWiFiCredentials();
	String nameStr = oldInfo.name;
	saveWiFiCredentials(ssidStr, passwordStr, nameStr);
}

void Persistent_Store::saveWiFiCredentials(String ssidStr, String passwordStr, String nameStr) {
	Serial.println("Entering saveWiFiCredentials");

	char ssid[32] = "";
	char password[32] = "";
	char name[32] = "";

	ssidStr.toCharArray(ssid, sizeof(ssid) - 1);
	passwordStr.toCharArray(password, sizeof(password) - 1);
	nameStr.toCharArray(name, sizeof(name) - 1);

	EEPROM.begin(512);
	EEPROM.put(0, ssid);
	EEPROM.put(0 + sizeof(ssid), password);
	EEPROM.put(0 + sizeof(ssid) + sizeof(password), name);
	char ok[2 + 1] = "OK";
	EEPROM.put(0 + sizeof(ssid) + sizeof(password) + sizeof(name), ok);
	EEPROM.commit();
	EEPROM.end();
}


WiFiInfo Persistent_Store::loadWiFiCredentials() {
	//Serial.println("Entering loadWiFiCredentials");

	char ssid[32] = "";
	char password[32] = "";
	char name[32] = "";

	EEPROM.begin(512);
	EEPROM.get(0, ssid);
	EEPROM.get(0 + sizeof(ssid), password);
	EEPROM.get(0 + sizeof(ssid) + sizeof(password), name);
	char ok[2 + 1];
	EEPROM.get(0 + sizeof(ssid) + sizeof(password) + sizeof(name), ok);
	EEPROM.end();
	if (String(ok) != String("OK")) {
		ssid[0] = 0;
		password[0] = 0;
	}

	//Serial.println("Recovered credentials:");
	//Serial.print("SSID: ");
	//Serial.println(ssid);
	//Serial.print("Password: ");
	//Serial.println(password);
	//Serial.print("Name: ");
	//Serial.println(name);

	WiFiInfo info;
	strcpy(info.ssid, ssid);
	strcpy(info.password, password);
	strcpy(info.name, name);

	return info;
}

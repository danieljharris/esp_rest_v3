#include "ESP_Device.h"



ESP_Device::ESP_Device()
{
	strcpy(masterInfo.ssid, MASTER_SSID);
	strcpy(masterInfo.password, MASTER_PASS);
	strcpy(masterInfo.name, MASTER_NAME);
}


ESP_Device::~ESP_Device()
{
}


bool ESP_Device::connectToWiFi(WiFiInfo info, WiFiMode wifiMode) {
	char* ssid = info.ssid;
	char* password = info.password;
	char* name = info.name;

	Serial.println("Connecting to:");
	Serial.print("SSID: ");
	Serial.println(ssid);
	Serial.print("Password: ");
	Serial.println(password);
	Serial.print("Name: ");
	Serial.println(name);

	WiFi.mode(wifiMode);
	WiFi.hostname(name);
	WiFi.begin(ssid, password);

	// Wait for connection
	int i = 0;
	while (WiFi.status() != WL_CONNECTED && i < 15) {
		delay(500);
		Serial.print(".");
		i++;
	}

	Serial.println("");

	if (WiFi.status() == WL_CONNECTED) {
		Serial.println("Connected to: ");
		Serial.print("SSID: ");
		Serial.println(ssid);
		Serial.print("IP address: ");
		Serial.println(WiFi.localIP().toString());

		return true;
	}
	else {
		Serial.println("Could not connect!");

		return false;
	}
}

bool ESP_Device::lookForMaster() {
	Serial.println("Entering findMaster");

	int networksFound = WiFi.scanNetworks();

	bool found = false;

	String lookingFor = masterInfo.ssid;

	Serial.print("Looking for: ");
	Serial.println(lookingFor);

	Serial.println("I Found these networks:");
	for (int i = 0; i < networksFound; i++)
	{
		String current_ssid = WiFi.SSID(i).c_str();

		Serial.print("SSID: ");
		Serial.println(current_ssid);

		if (current_ssid.equals(lookingFor) == true) {
			found = true;
		}
	}

	Serial.print("Master found: ");
	Serial.println(found);
	return found;
}

#include "ESP_Device.h"

ESP_Device::ESP_Device()
{
	strcpy(masterInfo.ssid, MASTER_SSID);
	strcpy(masterInfo.password, MASTER_PASS);
	strcpy(masterInfo.name, MASTER_NAME);
}

void ESP_Device::startDevice()
{
	//Make sure all WiFi and servers are off
	WiFi.softAPdisconnect(true);
	WiFi.disconnect(true);

	WiFi.mode(WIFI_STA);
	delay(500);

	//If master found: connect
	if (lookForMaster() == true) {
		if (connectToWiFi(masterInfo, WIFI_STA) == true) {
			if (getAndSaveMainWiFiInfo() == true) {
				if (connectToWiFi(new Persistent_Store->loadWiFiCredentials(), WIFI_STA) == true) {
					if (swapMode(new Client_Mode)) {
						enableOTAUpdates();
					}
					else { resetFunc(); }
				}
				else { resetFunc(); }
			}
			else { resetFunc(); }
		}
		else { resetFunc(); }
	}
	//If not: try to remember WiFi. If not: enter WiFi config mode. Then become master
	else {
		if (rememberWiFiSettings() == true) {

			//Preps to use static IP
			WiFi.config(*STATIC_IP, *GATEWAY, *SUBNET);

			if (connectToWiFi(new Persistent_Store->loadWiFiCredentials(), WIFI_AP_STA) == true) {
				if (swapMode(new Client_Mode)) {
					enableOTAUpdates();
				}
				else { resetFunc(); }
			}
			else { resetFunc(); }

			if (swapMode(new Master_Mode)) {

				//Starts up MDNS
				MDNS.begin(masterInfo.name);

				//Starts access point for new devices to connect to
				WiFi.softAP(masterInfo.ssid, masterInfo.password);

				enableOTAUpdates();
			}
			else { swapMode(new Config_Mode); }
		}
		else { swapMode(new Config_Mode); }
	}
}

void ESP_Device::handle()
{
	if (currentMode->getMode == Config) {
		if (lookForMaster() == true) resetFunc();

		currentMode->handle();
	}
	else {
		ArduinoOTA.handle();
		currentMode->handle();

		if (currentMode->getMode == Master) {
			if (lookupCountdown != 0) {
				lookupCountdown--;
			}
			else {
				lookupCountdown = lookupCountdownMax;
				refreshLookup();
			}
		}
	}
}


bool ESP_Device::swapMode(WiFi_Mode *newMode) {
	currentMode->stop();
	currentMode = newMode;
	return currentMode->start();
}

bool ESP_Device::getAndSaveMainWiFiInfo() {
	Serial.println("Entering getAndSaveMainWiFiInfo");

	String ip = WiFi.gatewayIP().toString() + ":" + MASTER_PORT;
	String url = "http://" + ip + "/wifi_info";

	HTTPClient http;
	http.begin(url);

	if (http.GET() == HTTP_CODE_OK) {
		String payload = http.getString();

		DynamicJsonBuffer jsonBuffer;
		JsonObject& input = jsonBuffer.parseObject(payload);
		String ssid = input["ssid"];
		String password = input["password"];

		new Persistent_Store->saveWiFiCredentials(ssid, password);

		http.end();
		return true;
	}
	else {
		http.end();
		return false;
	}
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

bool ESP_Device::rememberWiFiSettings() {
	Serial.println("Entering rememberWiFiSettings");

	WiFiInfo info = new Persistent_Store->loadWiFiCredentials();

	if (info.ssid != "" && info.password != "" && info.name != "") {
		return true;
	}
	else {
		return false;
	}
}

void ESP_Device::enableOTAUpdates() {
	Serial.println("Entering enableOTAUpdates");

	ArduinoOTA.setHostname("ESP8266");
	//ArduinoOTA.setPassword("esp8266");
	ArduinoOTA.begin();
}

/*
Name:		ESP_REST_V3.ino
Created:	05/08/2018 07:48:35 PM
Author:	    Daniel Harris
*/

#include "Persistent_Store.h"
#include "WiFi_Mode.h"
#include "Config.h"
#include "Client.h"
#include "Master.h"


/*
	Higher TODO:
	() Move function into classes and rework design
		(#) Create class for client
		(#) Create class for master
		() Create class for saving/loading info to ESP

	() Add a button to the config button to "Toggle" the power of the device (This can be used to distinguish which device it is!)
	() Add functionality to make clients automaticly become master if master gets disconnected

	Lower TODO:
	() Recreate SimpleSchema for C++ to validate Json input!
	() Update config to allow manual SSID to be entered if none found
	() Add apidoc for endpoints
	() Allow Master endpoints to be called using device IPs as well as Names (Will fix the issue of 2 devices having the same name)
	() Find a way of refreshing clientLookup without removing master from it (No need to remove master)
	() Try to reduce the sleep timers to make things faster
	() Look into implementing SmartConfig to connect devices to router
	() Add functionality for custom circuit board and input detection (Like PC's ESP)
	() Dont run turn on function if already on, and dont run turn off function if already off
	() Try to move some of the functions to seperate files (Get practice with hearder files)

	DONE:
	() If ESP does not reply when Master is doing get devices, then remove that ESP from connectedClients
	() Add master to its own connectedClients or master can not be accessed
	() Be able to change the name of the master ESP (Maybe see if the client server can be run at the same time as master?)
	() Populate clientLookup in becomeMaster
	() Restructure master endpoints to check IP first and not during the sending
	() Add endpoint that updates clientLookup without replying to user (If that is faster, if not dont bother)
	() Make sure all included libraries are needed/used
	() Make SSID a drop down menu in config site


	# = In Progress
	* = Finished
*/

int lookupCountdownMax = 100000;
int lookupCountdown = lookupCountdownMax;


const int MASTER_PORT = 235;

int gpioPin = 0;
bool gpioPinState = false;
bool configMode = false;
bool isMaster = false;


void setup() {
	Serial.begin(19200);
	Serial.println("Entering setup");

	pinMode(gpioPin, OUTPUT);
	digitalWrite(gpioPin, HIGH);

	//saveWiFiCredentials("PLUSNET-QJ75", "bf439b3bf2", "Master");
	//saveWiFiCredentials("", "bf439b3bf2", "Master");
	//saveWiFiCredentials("", "", "");
	//saveWiFiCredentials("BOB", "BOB", "BOB");

	boot();
}

void boot() {
	ESP_Device thisDevice();


	//Make sure all WiFi and servers are off
	WiFi.softAPdisconnect(true);
	WiFi.disconnect(true);

	WiFi.mode(WIFI_STA);
	delay(500);

	Config configMode;
	bool masterFound = configMode.lookForMaster();

	//If master found: connect
	if (masterFound == true) {
		if (configMode.connectToWiFi(masterInfo, WIFI_STA) == true) {
			configMode.~WiFi_Mode();
			if (configMode.getAndSaveMainWiFiInfo() == true) {
				if (current_mode.connectToWiFi(loadWiFiCredentials(), WIFI_STA) == true) {
					
					Client clientMode;
					current_mode = new Client();
					enableOTAUpdates();
				}
				else { setup(); }
			}
			else { setup(); }
		}
		else { setup(); }
	}
	//If not: try to remember WiFi. If not: enter WiFi config mode. Then become master
	else {
		if (rememberWiFiSettings() == true) {
			if (becomeMaster() == true) {
				enableOTAUpdates();
			}
			else { enterConfigMode(); }
		}
		else { enterConfigMode(); }
	}
}

void loop() {
	if (configMode == true) {
		if (findMaster()) setup();

		setupServer.handleClient();
	}
	else {
		ArduinoOTA.handle();

		if (isMaster == true) {
			masterServer.handleClient();

			if (lookupCountdown != 0) {
				lookupCountdown--;
			}
			else {
				lookupCountdown = lookupCountdownMax;
				refreshLookup();
			}
		}
		else {
			clientServer.handleClient();
		}
	}
}


bool rememberWiFiSettings(WiFi_Mode mode) {
	Serial.println("Entering rememberWiFiSettings");

	WiFiInfo info = new Persistent_Store->loadWiFiCredentials();

	if (info.ssid != "" && info.password != "" && info.name != "") {
		return true;
	}
	else {
		return false;
	}
}



void enableOTAUpdates() {
	Serial.println("Entering enableOTAUpdates");

	ArduinoOTA.setHostname("ESP8266");
	//ArduinoOTA.setPassword("esp8266");
	ArduinoOTA.begin();
}
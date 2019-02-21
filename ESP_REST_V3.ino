#include "Config.h"
#include "ClientServer.h"
#include "FrameworkServer.h"
#include "MasterServer.h"
#include "Setupserver.h"

#include "WiFiCredentials.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

/*
	TODO:
	() Add apidoc for endpoints
	() Add functionality to make clients automaticly become master if master gets disconnected
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
*/

typedef struct ReturnInfo {
	t_http_codes code = HTTP_CODE_NOT_FOUND;
	String body = "";
};

WiFiCredentials creds;

//int lookupCountdownMax = 100000;
//int lookupCountdown = lookupCountdownMax;
//std::vector<Device> clientLookup;

//Declareing functions
bool connectToWiFi(WiFiInfo info, WiFiMode wifiMode);

FrameworkServer* server = new FrameworkServer;

WiFiInfo masterInfo("ESP-REST-MASTER", "7kAtZZm9ak", "esp8266");

void setup() {
	Serial.begin(19200);
	Serial.println("\nEntering setup");

	//Sets the ESP's output pin to OFF
	pinMode(gpioPin, OUTPUT);
	digitalWrite(gpioPin, HIGH);

	//Make sure all WiFi and servers are off
	server->stop();
	WiFi.softAPdisconnect(true);
	WiFi.disconnect(true);

	WiFi.mode(WIFI_STA);
	//delay(500);

	//If master found: connect
	if (findAndConnectToMaster() == true) {
			server = new ClientServer();
			server->start();
	}
	//If not: try to remember WiFi. If not: enter WiFi config mode. Then become master
	else {
		if (canRememberWiFiSettings() == true) {
			server = new MasterServer();
			if (server->start() != true) {
				server = new SetupServer();
				server->start();
			}
		}
		else {
			server = new SetupServer();
			server->start();
		}
	}
}

void loop() {
	//if (configMode == true) {
	//	if (findMaster()) setup();
	//}
	ArduinoOTA.handle();
	server->handle();


	//else {
	//	ArduinoOTA.handle();

	//	if (isMaster == true) {
	//		server->server.handleClient();

	//		if (lookupCountdown != 0) {
	//			lookupCountdown--;
	//		}
	//		else {
	//			lookupCountdown = lookupCountdownMax;
	//			refreshLookup();
	//		}
	//	}
	//	else {
	//		server->server.handleClient();
	//		if (lookupCountdown != 0) {
	//			lookupCountdown--;
	//		}
	//		else {
	//			lookupCountdown = lookupCountdownMax;
	//			if (findMaster() == false) {
	//				Serial.println("Could not find master, electing new one");
	//				if (electNewMaster() == true) {
	//					Serial.println("I am new master!");
	//					setup();
	//				}
	//				else {
	//					Serial.println("I am not the new master");
	//					delay(20000); //Waits for 20 seconds for new master to connect
	//				}
	//			}
	//		}
	//	}
	//}
}


bool findMaster() {
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
bool canRememberWiFiSettings() {
	Serial.println("Entering canRememberWiFiSettings");

	WiFiInfo info = creds.load();

	return (info.ssid != "" && info.password != "" && info.hostname != "");
}
bool getAndSaveMainWiFiInfo() {
	Serial.println("Entering getAndSaveMainWiFiInfo");

	String ip = WiFi.gatewayIP().toString() + ":" + PORT;
	String url = "http://" + ip + "/wifi_info";

	HTTPClient http;
	http.begin(url);

	if (http.GET() == HTTP_CODE_OK) {
		String payload = http.getString();

		DynamicJsonBuffer jsonBuffer;
		JsonObject& input = jsonBuffer.parseObject(payload);
		String ssid = input["ssid"];
		String password = input["password"];

		creds.save(ssid, password);

		http.end();
		return true;
	}
	else {
		http.end();
		return false;
	}
}
bool findAndConnectToMaster() {
	return (findMaster() == true &&
			connectToWiFi(masterInfo, WIFI_STA) == true &&
			getAndSaveMainWiFiInfo() == true &&
			connectToWiFi(WIFI_STA) == true);
}




//Master functions



//Client functions




//Reused
bool connectToWiFi(WiFiMode wifiMode) {
	connectToWiFi(creds.load(), wifiMode);
}
bool connectToWiFi(WiFiInfo info, WiFiMode wifiMode) {
	Serial.println("Connecting to:");
	Serial.print("SSID: ");
	Serial.println(info.ssid);
	Serial.print("Password: ");
	Serial.println(info.password);
	Serial.print("Name: ");
	Serial.println(info.hostname);

	WiFi.mode(wifiMode);
	WiFi.hostname(info.hostname);
	WiFi.begin(info.ssid, info.password);

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
		Serial.println(info.ssid);
		Serial.print("IP address: ");
		Serial.println(WiFi.localIP().toString());

		return true;
	}
	else {
		Serial.println("Could not connect!");

		return false;
	}
}

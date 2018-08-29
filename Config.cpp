#include "Config.h"

Config::~Config()
{
	server.client().stop();
}


void Config::handleConfig() {
	Serial.println("Entering handleConfig");

	WiFiInfo info = new Persistent_Store->loadWiFiCredentials();

	//String ssid = info.ssid;
	String password = info.password;
	String name = info.name;

	String options = makeOptionsSSID();

	String content;
	content += "<html><body>";

	content += "<form action='/connect' method='POST'>Log in to Voice Controler:<br>";
	//content += "SSID:          <input type='text' name='SSID' placeholder='ssid found on router' value='" + ssid + "'><br>";

	content += "SSID:          <select name='SSID'>" + options + "</select><br>";

	content += "Password:      <input type='password' name='PASSWORD' placeholder='password found on router' value='" + password + "'><br>";
	content += "Device Name:   <input type='text' name='NAME' placeholder='device name' value='" + name + "'><br>";
	content += "<input type='submit' value='Connect'></form><br><br>";

	content += "</body></html>";
	server.send(HTTP_CODE_OK, "text/html", content);
}

void Config::handleConnect() {
	Serial.println("Entering handleConnect");
	String content;

	if (server.hasArg("SSID") && server.hasArg("PASSWORD") && server.hasArg("NAME")) {
		String strSsid = server.arg("SSID");
		String strPassword = server.arg("PASSWORD");
		String strName = server.arg("NAME");

		content += "<html><body>";
		content += "Connecting to given SSID and PASSWORD<br>";
		content += "SSID: " + strSsid + "<br>";
		content += "Password: ********<br>";
		content += "Name: " + strName + "<br>";
		content += "</body></html>";
		server.send(HTTP_CODE_OK, "text/html", content);

		delay(500);

		server.client().stop();
		WiFi.softAPdisconnect(true);
		WiFi.disconnect(true);

		delay(1000);

		configMode = false;

		new Persistent_Store->saveWiFiCredentials(strSsid, strPassword, strName);
		setup();
	}
	else {
		content += "<html><body>";
		content += "Unknown SSID, Password or Name<br>";
		content += "Please try again";
		content += "</body></html>";
		server.send(HTTP_CODE_OK, "text/html", content);
	}
}

String Config::makeOptionsSSID() {
	String returnOptions = "";

	int networksFound = WiFi.scanNetworks();
	for (int i = 0; i < networksFound; i++)
	{
		String ssid = WiFi.SSID(i).c_str();
		returnOptions += "<option value = '" + ssid + "'>" + ssid + "</option>";
	}

	return returnOptions;
}

bool Config::start()
{
	Serial.println("Entering config mode");

	//TODO: Fix this
	configMode = true;

	//Turn off old wifi
	server.client().stop(); // Stop is needed because we sent no content length
	WiFi.softAPdisconnect(true);
	WiFi.disconnect(true);
	WiFi.mode(WIFI_OFF);

	delay(500);

	//Turn on new wifi
	WiFi.mode(WIFI_AP);
	WiFi.softAP(SETUP_SSID, SETUP_PASSWORD);

	server.on("/", HTTP_GET, this->handleConfig);
	server.on("/connect", HTTP_GET, this->handleConnect);

	server.onNotFound(this->handleConfig);

	server.begin();

	return true;
}


#include "SetupServer.h"

bool SetupServer::start() {
	Serial.println("Entering config mode");

	WiFi.mode(WIFI_AP);
	WiFi.softAP(SETUP_SSID, SETUP_PASSWORD);

	addEndpoints();
	addUnknownEndpoint();
	server.begin(80);
}

void SetupServer::addEndpoints() {
	std::function<void()> setupConfig = handleSetupConfig();
	std::function<void()> setupConnect = handleSetupConnect();

	server.on("/", HTTP_GET, setupConfig);
	server.on("/connect", HTTP_GET, setupConnect);
}

std::function<void()> SetupServer::handleSetupConfig() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleSetupConfig");

		WiFiInfo info = creds.load();

		String password = info.password;
		String name = info.hostname;

		String options;
		int networksFound = WiFi.scanNetworks();
		for (int i = 0; i < WiFi.scanNetworks(); i++) {
			String ssid = WiFi.SSID(i).c_str();
			options += "<option value = '" + ssid + "'>" + ssid + "</option>";
		}

		String content;
		content += "<html><body>";

		content += "<form action='/connect' method='POST'>Log in to Voice Controler:<br>";

		content += "SSID:          <select name='SSID'>" + options + "</select><br>";

		content += "Password:      <input type='password' name='PASSWORD' placeholder='password found on router' value='" + password + "'><br>";
		content += "Device Name:   <input type='text' name='NAME' placeholder='device name' value='" + name + "'><br>";
		content += "<input type='submit' value='Connect'></form><br><br>";

		content += "</body></html>";
		server.send(HTTP_CODE_OK, "text/html", content);
	};
	return lambda;
}
std::function<void()> SetupServer::handleSetupConnect() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleSetupConnect");
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

			//delay(500);

			stop();
			WiFi.softAPdisconnect(true);
			WiFi.disconnect(true);

			//delay(1000);

			creds.save(strSsid, strPassword, strName);
			reset();
		}
		else {
			content += "<html><body>";
			content += "Unknown SSID, Password or Name<br>";
			content += "Please try again";
			content += "</body></html>";
			server.send(HTTP_CODE_OK, "text/html", content);
		}
	};
	return lambda;
}
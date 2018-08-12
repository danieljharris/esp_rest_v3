/*
Name:		ESP_REST_V3.ino
Created:	05/08/2018 07:48:35 PM
Author:	    Daniel Harris
*/

#include <String>
#include <vector>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>



/*
TODO:

() Add master to its own connectedClients or master can not be accessed
() If ESP does not reply when Master is doing get devices, then remove that ESP from connectedClients
() Allow Master endpoints to be called using device IPs as well as Names (Will fix the issue of 2 devices having the same name)
() Be able to change the name of the master ESP (Maybe see if the client server can be run at the same time as master?)

() Try to reduce the sleep timers to make things faster
() Make SSID a drop down menu in config site
() Look into implementing SmartConfig to connect devices to router
() Add functionality for custom circuit board and input detection (Like PC's ESP)

*/


typedef struct WiFiInfo {
	char ssid[32] = "";
	char password[32] = "";
	char name[32] = "Unknown";
};

typedef struct Device {
	String ip = "";
	String name = "Unknown";
};

std::vector<Device> clientLookup;

//Declareing functions
bool connectToWiFi(WiFiInfo info, WiFiMode wifiMode);
void saveWiFiCredentials(String nameStr);
void saveWiFiCredentials(String ssidStr, String passwordStr);
void saveWiFiCredentials(String ssidStr, String passwordStr, String nameStr);
WiFiInfo loadWiFiCredentials();
String getDeviceIPFromName(String name);
String getByName(String name, String path, t_http_codes expectedCode);
String getByIP(String ip, String path, t_http_codes expectedCode);
String postByName(String name, String path, t_http_codes expectedCode, JsonObject& jsonObject);

//Update these numbers per each device ***************************
const int MASTER_PORT = 235;
const char* SETUP_SSID = "ESP-REST-V3";
const char* SETUP_PASSWORD = "zJ2f5xSX";

//Config for when master connects to WiFi
IPAddress STATIC_IP(192, 168, 1, 100);
IPAddress GATEWAY(192, 168, 1, 254);
IPAddress SUBNET(255, 255, 255, 0);
//Update these numbers per each device ***************************


int gpioPin = 0;
bool gpioPinState = false;
bool configMode = false;
bool isMaster = false;

ESP8266WebServer setupServer(80);
ESP8266WebServer clientServer(80);
ESP8266WebServer masterServer(MASTER_PORT);

const String MDNS_ID = "ESP_NETWORK";

WiFiInfo masterInfo;
const char* MASTER_SSID = "ESP-REST-MASTER";
const char* MASTER_PASS = "7kAtZZm9ak";
const char* MASTER_NAME = "esp8266";


void setup() {
	Serial.println("Entering setup");

	//saveWiFiCredentials("PLUSNET-QJ75", "bf439b3bf2", "Master");
	//saveWiFiCredentials("", "bf439b3bf2", "Master");
	//saveWiFiCredentials("", "", "");
	//saveWiFiCredentials("BOB", "BOB", "BOB");

	pinMode(gpioPin, OUTPUT);
	digitalWrite(gpioPin, HIGH);

	Serial.begin(19200);

	strcpy(masterInfo.ssid, MASTER_SSID);
	strcpy(masterInfo.password, MASTER_PASS);
	strcpy(masterInfo.name, MASTER_NAME);

	//Make sure all WiFi and servers are off
	setupServer.client().stop();
	clientServer.client().stop();
	WiFi.softAPdisconnect(true);
	WiFi.disconnect(true);

	WiFi.mode(WIFI_STA);
	delay(500);
	bool masterFound = findMaster();

	//If master found: connect
	if (masterFound == true) {
		if (connectToWiFi(masterInfo, WIFI_STA) == true) {
			if (getAndSaveMainWiFiInfo() == true) {
				if (connectToWiFi(loadWiFiCredentials(), WIFI_STA) == true) {
					startClient();
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
				startClient();
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
			clientServer.handleClient();
		}
		else {
			clientServer.handleClient();
		}
	}
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

bool rememberWiFiSettings() {
	Serial.println("Entering rememberWiFiSettings");

	WiFiInfo info = loadWiFiCredentials();

	if (info.ssid != "" && info.password != "" && info.name != "") {
		return true;
	}
	else {
		return false;
	}
}


//Config
void enterConfigMode() {
	Serial.println("Entering config mode");

	configMode = true;

	//Turn off old wifi
	setupServer.client().stop(); // Stop is needed because we sent no content length
	WiFi.softAPdisconnect(true);
	WiFi.disconnect(true);
	WiFi.mode(WIFI_OFF);

	delay(500);

	//Turn on new wifi
	WiFi.mode(WIFI_AP);
	WiFi.softAP(SETUP_SSID, SETUP_PASSWORD);

	setupServer.on("/", handleConfig);
	setupServer.on("/connect", handleConnect);
	setupServer.onNotFound(handleConfig);

	setupServer.begin();
}

void handleConfig() {
	Serial.println("Entering handleConfig");

	WiFiInfo info = loadWiFiCredentials();

	String ssid = info.ssid;
	String password = info.password;
	String name = info.name;

	String content;
	content += "<html><body>";

	content += "<form action='/connect' method='POST'>Log in to Voice Controler:<br>";
	content += "SSID:          <input type='text' name='SSID' placeholder='ssid found on router' value='" + ssid + "'><br>";
	content += "Password:      <input type='password' name='PASSWORD' placeholder='password found on router' value='" + password + "'><br>";
	content += "Device Name:   <input type='text' name='NAME' placeholder='device name' value='" + name + "'><br>";
	content += "<input type='submit' value='Connect'></form><br><br>";

	content += "</body></html>";
	setupServer.send(HTTP_CODE_OK, "text/html", content);
}

void handleConnect() {
	Serial.println("Entering handleConnect");
	String content;

	if (setupServer.hasArg("SSID") && setupServer.hasArg("PASSWORD") && setupServer.hasArg("NAME")) {
		String strSsid = setupServer.arg("SSID");
		String strPassword = setupServer.arg("PASSWORD");
		String strName = setupServer.arg("NAME");

		content += "<html><body>";
		content += "Connecting to given SSID and PASSWORD<br>";
		content += "SSID: " + strSsid + "<br>";
		content += "Password: ********<br>";
		content += "Name: " + strName + "<br>";
		content += "</body></html>";
		setupServer.send(HTTP_CODE_OK, "text/html", content);

		delay(500);

		setupServer.client().stop();
		clientServer.client().stop();
		WiFi.softAPdisconnect(true);
		WiFi.disconnect(true);

		delay(1000);

		saveWiFiCredentials(strSsid, strPassword, strName);
		setup();
	}
	else {
		content += "<html><body>";
		content += "Unknown SSID, Password or Name<br>";
		content += "Please try again";
		content += "</body></html>";
		setupServer.send(HTTP_CODE_OK, "text/html", content);
	}
}


//Master functions
bool becomeMaster() {
	Serial.println("Entering becomeMaster");

	isMaster = false;

	WiFi.mode(WIFI_STA); //Turns on WIFI_STA for WiFi scanning
	delay(500);
	bool masterFound = findMaster();

	//Double checks that master is not available
	if (masterFound == false) {

		//Preps to use static IP
		WiFi.config(STATIC_IP, GATEWAY, SUBNET);

		WiFiInfo info = loadWiFiCredentials();

		if (connectToWiFi(info, WIFI_AP_STA) == false) {
			Serial.println("becomeMaster failed, could not connect");
			return false;
		}

		isMaster = true;

		//Starts up MDNS
		MDNS.begin(masterInfo.name);

		//Starts access point for new devices to connect to
		WiFi.softAP(masterInfo.ssid, masterInfo.password);

		//Master server setup
		masterServer.on("/wifi_info", HTTP_GET, handleMasterGetWiFiInfo);

		//Caller endpoints
		masterServer.on("/devices", HTTP_GET, handleMasterGetDevices);
		masterServer.on("/device", HTTP_POST, handleMasterSetDevice);
		masterServer.on("/name", HTTP_POST, handleMasterSetDeviceName);

		masterServer.onNotFound(handleMasterUnknown);
		masterServer.begin();
	}
	else {
		Serial.println("becomeMaster failed, master already exists");
		isMaster = false;
	}

	return isMaster;
}

void handleMasterGetWiFiInfo() {
	Serial.println("Entering handleMasterGetWiFiInfo");

	DynamicJsonBuffer outputBuffer;
	JsonObject& output = outputBuffer.createObject();

	WiFiInfo info = loadWiFiCredentials();
	output["ssid"] = info.ssid;
	output["password"] = info.password;

	String content;
	output.printTo(content);

	masterServer.send(HTTP_CODE_OK, "application/json", content);
}

void handleMasterGetDevices() {
	Serial.println("Entering handleMasterGetDevices");

	DynamicJsonBuffer outputBuffer;
	JsonObject& output = outputBuffer.createObject();
	JsonArray& devices = output.createNestedArray("devices");

	Serial.println("Sending mDNS query");
	int amountOfDevicesFound = MDNS.queryService(MDNS_ID, "tcp"); // Send out query for esp tcp services
	Serial.println("mDNS query done");

	//Clears known clients/devices ready to repopulate the vector
	clientLookup.clear();

	//Adds master (itself) to clientLookup
	WiFiInfo info = loadWiFiCredentials();
	Device myself;
	myself.name = info.name;
	myself.ip = WiFi.localIP().toString();
	clientLookup.push_back(myself);

	//Adds master (itself) to returning json array
	devices.add(getByIP(myself.ip, "/device", HTTP_CODE_OK));

	if (amountOfDevicesFound == 0) {
		Serial.println("no services found");
	}
	else {
		Serial.print(amountOfDevicesFound);
		Serial.println(" service(s) found");
		for (int i = 0; i < amountOfDevicesFound; ++i) {

			//Prints details for each service found
			Serial.print(i + 1);
			Serial.print(": ");
			Serial.print(MDNS.hostname(i));
			Serial.print(" (");
			Serial.print(MDNS.IP(i));
			Serial.println(")");

			//Call the device's IP and get its info
			String ip = MDNS.IP(i).toString();
			String reply = getByIP(ip, "/device", HTTP_CODE_OK);

			if (reply.equals("error") == false) {
				DynamicJsonBuffer intputBuffer;
				JsonObject& input = intputBuffer.parseObject(reply);

				Device newDevice;
				String name = input["name"];
				newDevice.ip = ip;
				newDevice.name = name;

				clientLookup.push_back(newDevice);
				devices.add(input);
			}
		}
	}

	String result;
	output.printTo(result);

	masterServer.send(HTTP_CODE_OK, "application/json", result);
}

void handleMasterSetDevice() {
	Serial.println("Entering handleMasterSetDevice");

	if (masterServer.hasArg("plain") == false)
	{
		masterServer.send(HTTP_CODE_BAD_REQUEST);
		return;
	}

	//Get json from caller
	DynamicJsonBuffer intputBuffer;
	JsonObject& input = intputBuffer.parseObject(masterServer.arg("plain"));
	String name = input["name"];
	String action = input["action"];
	bool powered = input["power"];

	//Prep json to send from master to client
	DynamicJsonBuffer outputBuffer;
	JsonObject& output = outputBuffer.createObject();
	output["action"] = action;
	output["power"] = powered;

	String reply = postByName(name, "/device", HTTP_CODE_OK, output);

	if (reply.equals("error") == false) {
		//Sends json from client to caller
		masterServer.send(HTTP_CODE_OK, "application/json", reply);
	}
	else {
		//If cant reach client, reply to caller with error
		masterServer.send(HTTP_CODE_BAD_GATEWAY, "application/json");
	}
}

void handleMasterSetDeviceName() {
	Serial.println("Entering handleMasterSetDeviceName");

	if (masterServer.hasArg("plain") == false)
	{
		masterServer.send(HTTP_CODE_BAD_REQUEST);
		return;
	}

	//Get json from caller
	DynamicJsonBuffer intputBuffer;
	JsonObject& input = intputBuffer.parseObject(masterServer.arg("plain"));
	String oldName = input["old_name"];
	String newName = input["new_name"];

	//Prep json to send from master to client
	DynamicJsonBuffer outputBuffer;
	JsonObject& output = outputBuffer.createObject();
	output["new_name"] = newName;

	String reply = postByName(oldName, "/name", HTTP_CODE_OK, output);

	if (reply.equals("error") == false) {
		//Sends json from client to caller
		masterServer.send(HTTP_CODE_OK, "application/json", reply);
	}
	else {
		//If cant reach client, reply to caller with error
		masterServer.send(HTTP_CODE_BAD_GATEWAY, "application/json");
	}
}

void handleMasterUnknown() {
	String UnknownUri = masterServer.uri();
	Serial.print("Unknown command: ");
	Serial.println(UnknownUri);
	masterServer.send(404);
}


//Client functions
bool startClient() {
	Serial.println("Entering startClient");

	//Starts up MDNS
	char hostString[16] = { 0 };
	sprintf(hostString, "ESP_%06X", ESP.getChipId());
	MDNS.begin(hostString);

	//Broadcasts IP so can be seen by master
	MDNS.addService(MDNS_ID, "tcp", 80);

	//Client server setup
	clientServer.on("/device", HTTP_GET, handleClientGetInfo);
	clientServer.on("/device", HTTP_POST, handleClientSetDevice);
	clientServer.on("/name", HTTP_POST, handleClientSetName);

	clientServer.onNotFound(handleClientUnknown);
	clientServer.begin();
}

bool getAndSaveMainWiFiInfo() {
	Serial.println("Entering getAndSaveMainWiFiInfo");

	String ip = WiFi.gatewayIP().toString() + ":" + MASTER_PORT;
	String url = "http://" + ip + "/wifi_info";

	HTTPClient http;
	http.begin(url);

	if (http.GET() == HTTP_CODE_OK) {
		String payload = http.getString();

		Serial.print("From Master - payload: ");
		Serial.println(payload);

		DynamicJsonBuffer intputBuffer;
		JsonObject& input = intputBuffer.parseObject(payload);
		String ssid = input["ssid"];
		String password = input["password"];

		Serial.print("From Master - SSID: ");
		Serial.println(ssid);
		Serial.print("From Master - PASSWORD: ");
		Serial.println(password);

		saveWiFiCredentials(ssid, password);

		http.end();
		return true;
	}
	else {
		http.end();
		return false;
	}
}

void handleClientGetInfo() {
	Serial.println("Entering handleClientGetInfo");

	DynamicJsonBuffer outputBuffer;
	JsonObject& output = outputBuffer.createObject();

	WiFiInfo info = loadWiFiCredentials();
	output["name"] = info.name;
	output["ip"] = WiFi.localIP().toString();
	output["powered"] = gpioPinState;

	String result;
	output.printTo(result);

	Serial.print("Replying with: ");
	Serial.println(result);

	clientServer.send(HTTP_CODE_OK, "application/json", result);
}

void handleClientSetDevice() {
	Serial.println("Entering handleClientSetDevice");

	if (clientServer.hasArg("plain") == false)
	{
		clientServer.send(HTTP_CODE_BAD_REQUEST);
		return;
	}

	DynamicJsonBuffer intputBuffer;
	JsonObject& input = intputBuffer.parseObject(clientServer.arg("plain"));

	bool lastState = gpioPinState;

	if (input["action"] == "toggle") {
		power_toggle();
	}
	else if (input["action"] == "set") {
		if (input["power"] == true) {
			power_on();
		}
		else if (input["power"] == false) {
			power_off();
		}
		else {
			clientServer.send(HTTP_CODE_BAD_REQUEST);
			return;
		}
	}
	else {
		clientServer.send(HTTP_CODE_BAD_REQUEST);
		return;
	}

	DynamicJsonBuffer outputBuffer;
	JsonObject& output = outputBuffer.createObject();

	output["power"] = gpioPinState;
	output["last_state"] = lastState;

	String result;
	output.printTo(result);

	clientServer.send(HTTP_CODE_OK, "application/json", result);
}

void handleClientSetName() {
	Serial.println("Entering handleClientSetName");

	if (clientServer.hasArg("plain") == false)
	{
		clientServer.send(HTTP_CODE_BAD_REQUEST);
		return;
	}

	DynamicJsonBuffer intputBuffer;
	JsonObject& input = intputBuffer.parseObject(clientServer.arg("plain"));
	String newName = input["new_name"];

	saveWiFiCredentials(newName);

	clientServer.send(HTTP_CODE_OK, "application/json");
}

void handleClientUnknown() {
	String UnknownUri = clientServer.uri();
	Serial.print("Unknown command: ");
	Serial.println(UnknownUri);
	clientServer.send(404);
}




//Reused
bool connectToWiFi(WiFiInfo info, WiFiMode wifiMode) {
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

void saveWiFiCredentials(String nameStr) {
	WiFiInfo oldInfo = loadWiFiCredentials();
	String ssidStr = oldInfo.ssid;
	String passwordStr = oldInfo.password;
	saveWiFiCredentials(ssidStr, passwordStr, nameStr);
}

void saveWiFiCredentials(String ssidStr, String passwordStr) {
	WiFiInfo oldInfo = loadWiFiCredentials();
	String nameStr = oldInfo.name;
	saveWiFiCredentials(ssidStr, passwordStr, nameStr);
}

void saveWiFiCredentials(String ssidStr, String passwordStr, String nameStr) {
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

WiFiInfo loadWiFiCredentials() {
	Serial.println("Entering loadWiFiCredentials");

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

	Serial.println("Recovered credentials:");
	Serial.print("SSID: ");
	Serial.println(ssid);
	Serial.print("Password: ");
	Serial.println(password);
	Serial.print("Name: ");
	Serial.println(name);

	WiFiInfo info;
	strcpy(info.ssid, ssid);
	strcpy(info.password, password);
	strcpy(info.name, name);

	return info;
}

void enableOTAUpdates() {
	Serial.println("Entering enableOTAUpdates");

	ArduinoOTA.setHostname("ESP8266");
	//ArduinoOTA.setPassword("esp8266");
	ArduinoOTA.begin();
}

String getDeviceIPFromName(String name) {
	for (std::vector<Device>::iterator it = clientLookup.begin(); it != clientLookup.end(); ++it) {
		Device device = *it;

		if (device.name.equals(name) == true) {
			return device.ip;
		}
	}

	return "";
}

String getByName(String name, String path, t_http_codes expectedCode) {
	String ip = getDeviceIPFromName(name);
	return getByIP(ip, path, expectedCode);
}

String getByIP(String ip, String path, t_http_codes expectedCode) {
	String url = "http://" + ip + ":80" + path;

	HTTPClient http;
	http.begin(url);

	String toReturn = "";

	if (http.GET() == expectedCode) {
		toReturn = http.getString();
	}
	else {
		toReturn = "error";
	}

	http.end();

	return toReturn;
}

String postByName(String name, String path, t_http_codes expectedCode, JsonObject& jsonObject) {
	String ip = getDeviceIPFromName(name);
	String url = "http://" + ip + ":80" + path;

	String payload;
	jsonObject.printTo(payload);

	HTTPClient http;
	http.begin(url);

	String toReturn = "";

	if (http.POST(payload) == expectedCode) {
		toReturn = http.getString();
	}
	else {
		toReturn = "error";
	}

	http.end();

	return toReturn;
}

//Power control
void power_toggle() {
	Serial.println("Entering power_toggle");

	if (gpioPinState == false) power_on();
	else power_off();
}

void power_on() {
	Serial.println("Entering power_on");

	gpioPinState = true;
	digitalWrite(gpioPin, LOW);
}

void power_off() {
	Serial.println("Entering power_off");

	gpioPinState = false;
	digitalWrite(gpioPin, HIGH);
}
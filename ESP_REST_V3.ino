#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
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

typedef struct WiFiInfo {
	char ssid[32] = "";
	char password[32] = "";
	char name[32] = "Unknown";
};

typedef struct Device {
	String ip = "";
	String name = "Unknown";
};

typedef struct ReturnInfo {
	t_http_codes code = HTTP_CODE_NOT_FOUND;
	String body = "";
};

int lookupCountdownMax = 100000;
int lookupCountdown = lookupCountdownMax;
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
String clientGetInfo();
ReturnInfo clientSetDevice(String inputStr);

const int MASTER_PORT = 235;
const char* SETUP_SSID = getDeviceHostName();
const char* SETUP_PASSWORD = "zJ2f5xSX";

//Config for when master connects to WiFi
IPAddress STATIC_IP(192, 168, 1, 100);
IPAddress GATEWAY(192, 168, 1, 254);
IPAddress SUBNET(255, 255, 255, 0);

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

	//Sets the ESP's output pin to OFF
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
			if (lookupCountdown != 0) {
				lookupCountdown--;
			}
			else {
				lookupCountdown = lookupCountdownMax;
				if (findMaster() == false) {
					if (electNewMaster() == true) {
						Serial.print("I am new master!");
						setup();
					}
					else {
						Serial.print("I am not the new master");
						delay(20000); //Waits for 20 seconds for new master to connect
					}
				}
			}
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

	//String ssid = info.ssid;
	String password = info.password;
	String name = info.name;

	String options = makeOptionsSSID();

	String content;
	content += "<html><body>";

	content += "<form action='/connect' method='POST'>Log in to Voice Controler:<br>";

	content += "SSID:          <select name='SSID'>" + options + "</select><br>";

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

		configMode = false;

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

String makeOptionsSSID() {
	String returnOptions = "";

	int networksFound = WiFi.scanNetworks();
	for (int i = 0; i < networksFound; i++)
	{
		String ssid = WiFi.SSID(i).c_str();
		returnOptions += "<option value = '" + ssid + "'>" + ssid + "</option>";
	}

	return returnOptions;
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

		refreshLookup();
	}
	else {
		Serial.println("becomeMaster failed, master already exists");
		isMaster = false;
	}

	return isMaster;
}

void handleMasterGetWiFiInfo() {
	Serial.println("Entering handleMasterGetWiFiInfo");

	DynamicJsonBuffer jsonBuffer;
	JsonObject& output = jsonBuffer.createObject();

	WiFiInfo info = loadWiFiCredentials();
	output["ssid"] = info.ssid;
	output["password"] = info.password;

	String content;
	output.printTo(content);

	masterServer.send(HTTP_CODE_OK, "application/json", content);
}

void handleMasterGetDevices() {
	Serial.println("Entering handleMasterGetDevices");

	DynamicJsonBuffer jsonBuffer;
	JsonObject& output = jsonBuffer.createObject();
	JsonArray& devices = output.createNestedArray("devices");

	for (std::vector<Device>::iterator it = clientLookup.begin(); it != clientLookup.end(); ++it) {
		Device device = *it;

		if (isMyIp(device.ip) == true) {
			JsonObject& masterInput = jsonBuffer.parseObject(clientGetInfo());
			devices.add(masterInput);
		}
		else {
			String reply = getByIP(device.ip, "/device", HTTP_CODE_OK);
			if (reply.equals("error") == false) {
				devices.add(jsonBuffer.parseObject(reply));
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

	DynamicJsonBuffer jsonBuffer;

	//Get json from caller
	JsonObject& input = jsonBuffer.parseObject(masterServer.arg("plain"));
	String name = input["name"];
	String action = input["action"];
	bool powered = input["power"];

	//Prep json to send from master to client
	JsonObject& output = jsonBuffer.createObject();
	output["action"] = action;
	output["power"] = powered;

	if (isMyName(name) == true) {
		String result;
		output.printTo(result);

		ReturnInfo returnInfo = clientSetDevice(result);
		masterServer.send(returnInfo.code, "application/json", returnInfo.body);
	}
	else {
		String reply = -(name, "/device", HTTP_CODE_OK, output);

		if (reply.equals("error") == false) {
			//Sends json from client to caller
			masterServer.send(HTTP_CODE_OK, "application/json", reply);
		}
		else {
			//If cant reach client, reply to caller with error
			masterServer.send(HTTP_CODE_BAD_GATEWAY, "application/json");
		}
	}
}

void handleMasterSetDeviceName() {
	Serial.println("Entering handleMasterSetDeviceName");

	if (masterServer.hasArg("plain") == false)
	{
		masterServer.send(HTTP_CODE_BAD_REQUEST);
		return;
	}

	DynamicJsonBuffer jsonBuffer;

	//Get json from caller
	JsonObject& input = jsonBuffer.parseObject(masterServer.arg("plain"));
	String oldName = input["old_name"];
	String newName = input["new_name"];

	if (isMyName(oldName) == true) {
		clientSetName(newName);
		masterServer.send(HTTP_CODE_OK, "application/json");
	}
	else {
		//Prep json to send from master to client
		JsonObject& output = jsonBuffer.createObject();
		output["new_name"] = newName;

		String reply = postByName(oldName, "/name", HTTP_CODE_OK, output);

		if (reply.equals("error") == false) {
			//Sends json from client to caller
			masterServer.send(HTTP_CODE_OK, "application/json");
		}
		else {
			//If cant reach client, reply to caller with error
			masterServer.send(HTTP_CODE_BAD_GATEWAY, "application/json");
		}
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
	strcat(hostString, getDeviceHostName());
	MDNS.begin(hostString);

	Serial.println(hostString);

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

		DynamicJsonBuffer jsonBuffer;
		JsonObject& input = jsonBuffer.parseObject(payload);
		String ssid = input["ssid"];
		String password = input["password"];

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

	String result = clientGetInfo();

	clientServer.send(HTTP_CODE_OK, "application/json", result);
}

String clientGetInfo() {
	Serial.println("Entering clientGetInfo");

	DynamicJsonBuffer jsonBuffer;
	JsonObject& output = jsonBuffer.createObject();

	WiFiInfo info = loadWiFiCredentials();
	output["name"] = info.name;
	output["ip"] = WiFi.localIP().toString();
	output["powered"] = gpioPinState;

	String result;
	output.printTo(result);

	return result;
}

void handleClientSetDevice() {
	Serial.println("Entering handleClientSetDevice");

	if (clientServer.hasArg("plain") == false)
	{
		clientServer.send(HTTP_CODE_BAD_REQUEST);
		return;
	}

	ReturnInfo returnInfo = clientSetDevice(clientServer.arg("plain"));
	clientServer.send(returnInfo.code, "application/json", returnInfo.body);
}

ReturnInfo clientSetDevice(String inputStr) {
	Serial.println("Entering clientSetDevice");

	DynamicJsonBuffer jsonBuffer;
	JsonObject& input = jsonBuffer.parseObject(inputStr);

	bool lastState = gpioPinState;

	ReturnInfo returnInfo;

	if (input["action"] == "toggle") {
		power_toggle();
	}
	else if (input["action"] == "pulse") {
		power_on();
		delay(1000);
		power_off();
	}
	else if (input["action"] == "set") {
		if (input["power"] == true) {
			power_on();
		}
		else if (input["power"] == false) {
			power_off();
		}
		else {
			returnInfo.code = HTTP_CODE_BAD_REQUEST;
			return returnInfo;
		}
	}
	else {
		returnInfo.code = HTTP_CODE_BAD_REQUEST;
		return returnInfo;
	}

	JsonObject& output = jsonBuffer.createObject();

	output["power"] = gpioPinState;
	output["last_state"] = lastState;

	String result;
	output.printTo(result);

	returnInfo.code = HTTP_CODE_OK;
	returnInfo.body = result;

	return returnInfo;
}

void handleClientSetName() {
	Serial.println("Entering handleClientSetName");

	if (clientServer.hasArg("plain") == false)
	{
		clientServer.send(HTTP_CODE_BAD_REQUEST);
		return;
	}

	DynamicJsonBuffer jsonBuffer;
	JsonObject& input = jsonBuffer.parseObject(clientServer.arg("plain"));
	String newName = input["new_name"];

	clientSetName(newName);

	clientServer.send(HTTP_CODE_OK, "application/json");
}

void clientSetName(String newName) {
	Serial.println("Entering clientSetName");
	saveWiFiCredentials(newName);
}

void handleClientUnknown() {
	String UnknownUri = clientServer.uri();
	Serial.print("Unknown command: ");
	Serial.println(UnknownUri);
	clientServer.send(404);
}

bool electNewMaster() {
	Serial.print("Entering electNewMaster");
	//Initalises the chosen host name to the host name of the current device
	String myHostName = getDeviceHostName();

	String chosenHostName = myHostName;

	//Prints details for each service found
	Serial.print("My host name: ");
	Serial.print(chosenHostName);
	Serial.print("Other host names: ");

	//Send out query for ESP_REST devices
	int devicesFound = MDNS.queryService(MDNS_ID, "tcp");
	for (int i = 0; i < devicesFound; ++i) {
		String currentHostName = MDNS.hostname(i);
		if (currentHostName > chosenHostName) {
			chosenHostName = currentHostName;
		}
		Serial.print(currentHostName);
	}

	return myHostName.equals(chosenHostName);
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

	WiFiInfo info;
	strcpy(info.ssid, ssid);
	strcpy(info.password, password);
	strcpy(info.name, name);

	return info;
}

void enableOTAUpdates() {
	Serial.println("Entering enableOTAUpdates");

	//Enabled "Over The Air" updates so that the ESPs can be updated remotely 
	ArduinoOTA.setHostname("ESP8266");
	ArduinoOTA.begin();
}

String getDeviceIPFromName(String name) {
	for (std::vector<Device>::iterator it = clientLookup.begin(); it != clientLookup.end(); ++it) {
		Device device = *it;

		if (device.name.equalsIgnoreCase(name) == true) {
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
	return postByIP(ip, path, expectedCode, jsonObject);
}

String postByIP(String ip, String path, t_http_codes expectedCode, JsonObject& jsonObject) {
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

void refreshLookup() {
	//Clears known clients ready to repopulate the vector
	clientLookup.clear();

	//Adds master (itself) to clientLookup
	WiFiInfo info = loadWiFiCredentials();
	Device myself;
	myself.name = info.name;
	myself.ip = WiFi.localIP().toString();
	clientLookup.push_back(myself);

	//Send out query for ESP_REST devices
	int devicesFound = MDNS.queryService(MDNS_ID, "tcp");
	for (int i = 0; i < devicesFound; ++i) {
		//Call the device's IP and get its info
		String ip = MDNS.IP(i).toString();
		String reply = getByIP(ip, "/device", HTTP_CODE_OK);

		if (reply.equals("error") == false) {
			DynamicJsonBuffer jsonBuffer;
			JsonObject& input = jsonBuffer.parseObject(reply);

			Device newDevice;
			String name = input["name"];
			newDevice.ip = ip;
			newDevice.name = name;

			clientLookup.push_back(newDevice);
		}
	}
}

bool isMyIp(String ip) {
	return ip.equals( WiFi.localIP().toString() );
}

bool isMyName(String name) {
	String ip = getDeviceIPFromName(name);
	return isMyIp(ip);
}

char* getDeviceHostName() {
	char* newName = "ESP_";
	char hostName[10];
	sprintf(hostName, "%d", ESP.getChipId());
	strcat(newName, hostName);

	return newName;
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

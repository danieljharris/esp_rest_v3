#include "Master_Mode.h"

bool Master_Mode::start() {
	Serial.println("Entering becomeMaster");

	//Master server setup
	server.on("/wifi_info", HTTP_GET, this->handleGetWiFiInfo);

	//Caller endpoints
	server.on("/devices", HTTP_GET, this->handleGetDevices);
	server.on("/device", HTTP_POST, this->handleSetDevice);
	server.on("/name", HTTP_POST, this->handleSetDeviceName);

	server.onNotFound(this->handleUnknown);
	server.begin();

	refreshLookup();

	return true;
}

void Master_Mode::refreshLookup() {
	//Serial.println("Entering refreshLookup");

	//Clears known clients ready to repopulate the vector
	clientLookup.clear();

	//Adds master (itself) to clientLookup
	WiFiInfo info = new Persistent_Store->loadWiFiCredentials();
	Device myself;
	myself.name = info.name;
	myself.ip = WiFi.localIP().toString();
	clientLookup.push_back(myself);

	// Send out query for ESP_REST_V3 devices
	int devicesFound = MDNS.queryService(MDNS_ID, "tcp");
	for (int i = 0; i < devicesFound; ++i) {
		//Prints details for each service found
		//Serial.print(i + 1);
		//Serial.print(": ");
		//Serial.print(MDNS.hostname(i));
		//Serial.print(" (");
		//Serial.print(MDNS.IP(i));
		//Serial.println(")");

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

void Master_Mode::handleGetWiFiInfo() {
	Serial.println("Entering handleMasterGetWiFiInfo");

	DynamicJsonBuffer jsonBuffer;
	JsonObject& output = jsonBuffer.createObject();

	WiFiInfo info = new Persistent_Store->loadWiFiCredentials();
	output["ssid"] = info.ssid;
	output["password"] = info.password;

	String content;
	output.printTo(content);

	server.send(HTTP_CODE_OK, "application/json", content);
}

void Master_Mode::handleGetDevices() {
	Serial.println("Entering handleMasterGetDevices");

	DynamicJsonBuffer jsonBuffer;
	JsonObject& output = jsonBuffer.createObject();
	JsonArray& devices = output.createNestedArray("devices");

	for (std::vector<Device>::iterator it = clientLookup.begin(); it != clientLookup.end(); ++it) {
		Device device = *it;

		if (isMyIp(device.ip) == true) {

			DynamicJsonBuffer jsonBuffer;
			JsonObject& masterDevice = jsonBuffer.createObject();

			masterDevice["name"] = getName();
			masterDevice["powered"] = getState();
			masterDevice["ip"] = WiFi.localIP().toString();

			devices.add(masterDevice);
		}
		else {
			String reply = getByIP(device.ip, "/device", HTTP_CODE_OK);
			if (reply.equals("error") == false) {
				JsonObject& input = jsonBuffer.parseObject(reply);
				devices.add(input);
			}
		}
	}

	String result;
	output.printTo(result);

	server.send(HTTP_CODE_OK, "application/json", result);
}

void Master_Mode::handleSetDevice() {
	Serial.println("Entering handleMasterSetDevice");

	if (server.hasArg("plain") == false)
	{
		server.send(HTTP_CODE_BAD_REQUEST);
		return;
	}

	DynamicJsonBuffer jsonBuffer;

	//Get json from caller
	JsonObject& input = jsonBuffer.parseObject(server.arg("plain"));
	String name = input["name"];
	String action = input["action"];
	bool powered = input["power"];

	//Prep json to send from master to client
	JsonObject& output = jsonBuffer.createObject();
	output["action"] = action;
	output["power"] = powered;

	if (isMyName(name) == true) {
		handleSetState();
	}
	else {
		String reply = postByName(name, "/device", HTTP_CODE_OK, output);

		if (reply.equals("error") == false) {
			//Sends json from client to caller
			server.send(HTTP_CODE_OK, "application/json", reply);
		}
		else {
			//If cant reach client, reply to caller with error
			server.send(HTTP_CODE_BAD_GATEWAY, "application/json");
		}
	}
}

void Master_Mode::handleSetDeviceName() {
	Serial.println("Entering handleMasterSetDeviceName");

	if (server.hasArg("plain") == false)
	{
		server.send(HTTP_CODE_BAD_REQUEST);
		return;
	}

	DynamicJsonBuffer jsonBuffer;

	//Get json from caller
	JsonObject& input = jsonBuffer.parseObject(server.arg("plain"));
	String oldName = input["old_name"];
	String newName = input["new_name"];

	if (isMyName(oldName) == true) {
		handleSetName();
	}
	else {
		//Prep json to send from master to client
		JsonObject& output = jsonBuffer.createObject();
		output["new_name"] = newName;

		String reply = postByName(oldName, "/name", HTTP_CODE_OK, output);

		if (reply.equals("error") == false) {
			//Sends json from client to caller
			server.send(HTTP_CODE_OK, "application/json");
		}
		else {
			//If cant reach client, reply to caller with error
			server.send(HTTP_CODE_BAD_GATEWAY, "application/json");
		}
	}
}

String Master_Mode::getDeviceIPFromName(String name) {
	for (std::vector<Device>::iterator it = clientLookup.begin(); it != clientLookup.end(); ++it) {
		Device device = *it;

		if (device.name.equalsIgnoreCase(name) == true) {
			return device.ip;
		}
	}

	return "";
}

bool Master_Mode::isMyIp(String ip) {
	return ip.equals(WiFi.localIP().toString());
}

bool Master_Mode::isMyName(String name) {
	String ip = getDeviceIPFromName(name);
	return isMyIp(ip);
}
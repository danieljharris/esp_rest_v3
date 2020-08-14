//FrameworkServer.cpp

#include "FrameworkServer.h"

FrameworkServer::~FrameworkServer() { stop(); }

void FrameworkServer::stop() {
	Serial.println("Entering stop");

	server.close();
	server.stop();
}

char* FrameworkServer::getDeviceHostName() {
	char* newName = "ESP_";
	char hostName[10];
	sprintf(hostName, "%d", ESP.getChipId());
	strcat(newName, hostName);

	return newName;
}

void FrameworkServer::enableOTAUpdates() {
	//Enabled "Over The Air" updates so that the ESPs can be updated remotely 
	ArduinoOTA.setPort(8266);
	ArduinoOTA.setHostname(getDeviceHostName());

	ArduinoOTA.onStart([]() {
		MDNS.close(); //Close to not interrupt other devices
		Serial.println("OTA Update Started...");
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nOTA Update Ended!");
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
	});

	ArduinoOTA.begin();
}

void FrameworkServer::addUnknownEndpoint() {
	std::function<void()> lambda = [=]() {
		Serial.println("Entering handleClientUnknown");

		Serial.println("");
		Serial.print("Unknown command: ");
		Serial.println(server.uri());

		Serial.print("Method: ");
		Serial.println(server.method());

		server.send(HTTP_CODE_NOT_FOUND);
	};
	server.onNotFound(lambda);
}

void FrameworkServer::addEndpoints(std::vector<Endpoint> endpoints) {
	for (std::vector<Endpoint>::iterator it = endpoints.begin(); it != endpoints.end(); ++it) {
		server.on(it->path, it->method, it->function);
	}
}

void FrameworkServer::selfRegister() {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

	json["id"] = ESP.getChipId();
	json["name"] = creds.load().hostname;

	String body;
	json.printTo(body);

	String ip = CLOUD_IP + ":" + CLOUD_PORT;
	String url = "http://" + ip + "/device";

	HTTPClient http;
	http.begin(url);
	http.POST(body);
	http.end();
}

void FrameworkServer::configUpdate() {
	WiFiClient client;
	if (!client.connect(CLOUD_IP, CLOUD_PORT)) return;
	client.print(String("GET /config?id=") + ESP.getChipId() + " HTTP/1.1\r\n" +
		"Host: " + CLOUD_IP + "\r\n" +
		"Connection: close\r\n" +
		"\r\n"
	);

	String payload;

	// Gets the last line of the message
	while (client.connected() || client.available()) {
		if (client.available()) payload = client.readStringUntil('\n');
	}
	client.stop();

	if (payload == "") return;

	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(payload);

	if (!json.success()
		|| !json.containsKey("dest_id")
		|| !json.containsKey("method")
		|| !json.containsKey("path")
		|| !json.containsKey("data")) return;

	String id     = json["dest_id"].asString();
	String method = json["method"].asString();
	String path   = json["path"].asString();

	String data = "";
	DynamicJsonBuffer().parseObject(json["data"].asString()).printTo(data);

	connected.save(id, method, path, data);
}

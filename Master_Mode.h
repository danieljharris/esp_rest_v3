#pragma once

#include "Client_Mode.h"

typedef struct Device {
	String ip = "";
	String name = "Unknown";
};

class Master_Mode : public Client_Mode
{
protected:
	modes currentMode = Master;

	vector<Device> clientLookup;

	void refreshLookup();
	void handleGetWiFiInfo();
	void handleGetDevices();
	void handleSetDevice();
	void handleSetDeviceName();

	String getDeviceIPFromName(String name);
	bool isMyIp(String ip);
	bool isMyName(String name);

public:
	bool start();
};


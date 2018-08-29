#pragma once

#include "Client.h"

typedef struct Device {
	String ip = "";
	String name = "Unknown";
};

class Master : public Client
{
private:
	String mode_name = "Master";

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
	Master();
	//~Master();

	bool becomeMaster();
};


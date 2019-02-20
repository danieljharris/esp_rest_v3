// SetupServer.h

#ifndef _SETUPSERVER_h
#define _SETUPSERVER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "FrameworkServer.h"

class SetupServer : public FrameworkServer {
private:
	const char* SETUP_SSID = getDeviceHostName();
	const char* SETUP_PASSWORD = "zJ2f5xSX";

	std::function<void()> handleSetupConfig();
	std::function<void()> handleSetupConnect();

public:
	bool start();
	void addEndpoints();
};

#endif


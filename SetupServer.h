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

	void addEndpoints();
	std::function<void()> handleSetupConfig();
	std::function<void()> handleSetupConnect();

public:
	bool start();
};

#endif


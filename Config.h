//Config.h

#pragma once
#ifndef _CONFIG_h
#define _CONFIG_h

#include "WiFiCredentials.h"

const WiFiInfo MASTER_INFO("Universal-Framework", "7kAtZZm9ak", "UniFrame");

const String SETUP_PASSWORD = "zJ2f5xSX";

const String MASTER_MDNS_ID = "UniFrameMaster";
const String CLIENT_MDNS_ID = "UniFrameClients";

const int SETUP_PORT = 80;
const int CLIENT_PORT = 80;
const int MASTER_PORT = 235;

const int GPIO_PIN = 0;

#endif
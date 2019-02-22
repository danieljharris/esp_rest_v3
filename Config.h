#pragma once
#ifndef _CONFIG_h
#define _CONFIG_h

#include "WiFiCredentials.h"
#include <IPAddress.h>

const WiFiInfo MASTER_INFO("Universal-Framework", "7kAtZZm9ak", "UniFrame");

const String MDNS_ID = "UNI_FRAME";

const int SETUP_PORT = 80;
const int CLIENT_PORT = 80;
const int MASTER_PORT = 235;

const int GPIO_PIN = 0;

#endif
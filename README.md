# Universal IoT Framework for ESP8266
Using this plug and play framework automaticly creates and maintains a REST network.
Allowing for decentralised handling of IoT devices.

The way it works is as follows:
1) The first ESP8266 powerd up will create a WiFi access point, you connect to this using your phone/WiFi enabled device. Then select a WiFi network from the drop down menu and enter the password. (At this point it is also useful to give a name to identify the device later)
2) The ESP8266 then connects to the given WiFi access point, and creats a new access point for future ESP8266 devices to use.
3) Any further ESP8266 devices powered on will now detect the first ESP8266, and will retrieve the WiFi login information from it, meaning you only need to enter you WiFi details once on a single device.
4) You can then access any device using the endpoints accessable at UniFrame:235 (http://UniFrame:235/devices returns entire network). All commands should be sent to this address, they are then automaticly distributed to the correct device.
5) If the first device fails for any reason (Such as loosing power) another device on the network will step in its place and become the new manager of the framework.

Some notes about how it works:
* All ESP8266 devices are communicated to via the first powered on ESP8266 (known as the master). Which device is the master is not relavant, as all devices are access the same.
* You communicate to the endpoints using Json and you specify which ESP8266 to use using its "name", which can be changed via an endpoint.

# NMEA2000 WiFi-Gateway - My Boat Edition

This project is a combination of three of mine other GitHub projects. It combines the [NMEA2000 WiFi-Gateway](https://github.com/AK-Homberger/NMEA2000WifiGateway-with-ESP32) with the Sonoff/Tasmota [power meter](https://github.com/AK-Homberger/M5Stack-Sonoff-Power-Display) and the [heater control](https://github.com/AK-Homberger/WLAN-Controlled-Heater-Thermostat-for-Tasmota-switch) project. That's the combination I'm using currently on my boat.

The ESP32 DevModule of the gateway hardware is replacing the M5Stack and the D1-Mini fom the other projects. The gateway functions are controlled with a web browser.

The same hardware and PCB as for the gateway is used. We only need one additional DS18B20 temperature sensor for the room temperature. And of course the Sonoff POW R2 as power meter and a Tasmota switch to control the heater. On both devices the [Tasmota](https://tasmota.github.io/docs/) software has to be installed. For details please have a look to the individual GitHub project pages.

The same libraries as for the gateway are needed. Just follow the instructions from the gateway GitHub project, but use [this software](https://github.com/AK-Homberger/NMEA2000-Gateway-My-Boat-Edition/tree/main/NMEA2000-Gateway-MBE) in the Arduino IDE.

If you want to change the software according to you own needs, then the information in the [NMEA2000 Workshop](https://github.com/AK-Homberger/NMEA2000-Workshop) might be useful for you.


## Settings
To adjust the program to your needs, you can change the settings in the sketch:
```
//****************************************************************************************
// Configuration start

#define ENABLE_DEBUG_LOG 0          // Debug log, set to 1 to enable AIS forward on USB-Serial / 2 for ADC voltage to support calibration
#define UDP_Forwarding 0            // Set to 1 for forwarding AIS from serial2 to UDP brodcast
#define HighTempAlarm 12            // Alarm level for fridge temperature (higher)
#define LowVoltageAlarm 11.5        // Alarm level for battery voltage (lower)
#define ADC_Calibration_Value 34.3  // The real value depends on the true resistor values for the ADC input (100K / 27 K)
#define WLAN_CLIENT 0               // Set to 1 to enable client network. 0 to act as AP only

// Wifi cofiguration Client and Access Point
const char *AP_ssid = "MyESP32";        // ESP32 as AP
const char *CL_ssid = "MyWLAN";         // ESP32 as client in network

const char *AP_password = "password";   // AP password
const char *CL_password = "password";   // Client password

// Put IP address details here
IPAddress AP_local_ip(192, 168, 15, 1); // Static address for AP
IPAddress AP_gateway(192, 168, 15, 1);
IPAddress AP_subnet(255, 255, 255, 0);

IPAddress CL_local_ip(192, 168, 1, 10); // Static address for Client Network. Please adjust to your AP IP and DHCP range!
IPAddress CL_gateway(192, 168, 1, 1);
IPAddress CL_subnet(255, 255, 255, 0);

const uint16_t ServerPort = 2222; // Define the TCP port, where server sends data. Use this e.g. on OpenCPN. Use 39150 for Navionis AIS

// UPD broadcast for Navionics, OpenCPN, etc.
const char* udpAddress = "192.168.15.255"; // UDP broadcast address. Must be in the network of the ESP32
const int udpPort = 2000; // port 2000

const char* POW_IP = "192.168.15.210";    // Server IP. Set Sonoff POW to fixed IP
const char* TasmotaIP = "192.168.15.200"; // Defines address of Tasmota heater switch

// Configuration end
//****************************************************************************************
```

It's important to configure the Sonoff POW R2 and the Tasmota heater switch with the above defined IP adresses. I'm using the sketch as Access Point and in that case no changes of IP adresses in the sketch should be necessary. If you want to use it as a client in an existing WLAN, you have to change the IP adresses according to your local needs.

## Main Page
The sketch is implementing mDNS and the Bonjour protocol. In a web client you can simply provide the name "gateway.local". Then the initial web page is shown:

![MainPage](https://github.com/AK-Homberger/NMEA2000-Gateway-My-Boat-Edition/blob/main/Pictures/MainPage.png)

With this main page you can access the other web pages to:

- Show navigation information
- Control the power supply
- Control the heater thermostat
- Show wind information (big)


## Navigation Data Page
The navigation web page shows essential navigational data received from the NMEA2000 network. No additional client app is needed on the phone/tablet or laptop.

![Navigation](https://github.com/AK-Homberger/NMEA2000-Gateway-My-Boat-Edition/blob/main/Pictures/Navigation.png)


## Power Control Page
The power control web page shows information regarding the board voltage (12 Volt), the fridge temperature and the mains power values.

![Power](https://github.com/AK-Homberger/NMEA2000-Gateway-My-Boat-Edition/blob/main/Pictures/PowerControl.png)

You can also switch on/off the alarm for mains power losses. And you can also reset the energy value to zero.

## Heater Control Page
With the heater control page you can control the heater.
![Heater](https://github.com/AK-Homberger/NMEA2000-Gateway-My-Boat-Edition/blob/main/Pictures/HeaterControl.png)

You can set the thermostat temperature and the state (Auto, On, Off).
In Auto mode the Gateway is controling the temperature by switching the heater on/off depending on the cabin temperature.

In addition to the fridge temperature we use an additional DS18B20 sensor to measure the cabin temperature. Just connect the new sensor in parallel to the first sensor.
In the sketch you have to define, which sensor is providing which temperature:

```
tmp = sensors.getTempCByIndex(1);      // First sensor
    if (tmp != -127) {
      if (abs(FTO - tmp) < 1) FridgeTemp = tmp;
      FTO = tmp;
    }

    tmp = sensors.getTempCByIndex(0);       // Second sensor
    if (tmp != -127) {
      if (abs(RTO - tmp) < 1) RoomTemp = tmp;
      RTO = tmp;
    }
```

Just set the index in "sensors.getTempCByIndex(1)" to "0" or "1". It would be possible to identify the individual sensor by the unique ID. But for only two sensors it's easier to just try out the two options with the index.

## Wind Page (Big)
The last page is just showing the current AWS and the maximum wind speed.
![Wind](https://github.com/AK-Homberger/NMEA2000-Gateway-My-Boat-Edition/blob/main/Pictures/AWS-Big.png)

# Other Apps / Mobile Connection
I'm using the gateway together with a [Navionics boating app](https://www.navionics.com/deu/apps/navionics-boating) and a [logbook app](https://logbook-app.com/de/) on my tablet.
For both apps the tcp connection via port 2222 works great.

But especially for the logbook app functionality (e.g. geoencoding and weather) it's important that the tablet can use the mobile connection in addition to the access point connection.

At least for Apple devices there is a simple trick: Just configure the gateway AP with a fixed IP address (e.g. 192.168.15.2) and no gateway.
In that case that the phone/tablet can use a mobile connection further in parallel.





/*
  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

// NMEA 2000 WiFi Gateway (My Boat Edition)
// Version 1.0, 23.12.2021, AK-Homberger

#define ESP32_CAN_TX_PIN GPIO_NUM_5  // Set CAN TX port to 5 (used in NMEA2000_CAN.H)
#define ESP32_CAN_RX_PIN GPIO_NUM_4  // Set CAN RX port to 4

#include <Arduino.h>
#include <NMEA2000_CAN.h>  // This will automatically choose the right CAN library and create suitable NMEA2000 object
#include <N2kMessages.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <OneButton.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// Local includes
#include "N2kDataToNMEA0183.h"
#include "List.h"
#include "index_html.h"
#include "nmea_html.h"
#include "power_html.h"
#include "heater_html.h"
#include "aws_big_html.h"
#include "BoatData.h"


#define SlowDataUpdatePeriod 1000  // Time between CAN Messages sent
#define MiscSendOffset 50

#define On true
#define Off false

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

// Power Meter - Sonoff POW R2
int no_connection = 0;               // Connection re-tries. Alarm if > 5
bool p_update = false;               // New data from POW
bool p_sharp = false;                // Power alarm On/Off
bool p_alarm = false;                // Alarm state

struct tPower {
  double Total, Yesterday, Today, Power, ApparentPower, ReactivePower, Factor, Voltage, Current;
};
tPower Power;

// Heater
#define Correction 0     // Defines corection value for heater temperature sensor
float TempLevel = 15;    // Initial temperature level. Current value is stored in NVS
float RoomTemp = 0;      // Current temperature
float Hyst = 0.1;        // Hysterese

bool OnOff = false;
bool Error = false;
bool Auto = false;
bool h_update = false;
unsigned long t, t2;

// Create UDP instance
WiFiUDP udp;

// Struct to update BoatData. See BoatData.h for content
tBoatData BoatData;

int NodeAddress;  // To store last Node Address

int wifiType = 0; // 0= Client 1= AP, Is set in setup()

Preferences preferences;  // Nonvolatile storage on ESP32 - To store LastDeviceAddress

int buzzerPin = 12;       // Buzzer on GPIO 12
int buttonPin = 0;        // Button on GPIO 0 to acknowledge alarm with buzzer
int alarmstate = false;   // Alarm state (low voltage, temperature, power loss)
int acknowledge = false;  // Acknowledge for alarm, button pressed

OneButton button(buttonPin, false); // The OneButton library is used to debounce the acknowledge button

const size_t MaxClients = 10;
bool SendNMEA0183Conversion = true; // Do we send NMEA2000 -> NMEA0183 conversion

WiFiServer server(ServerPort, MaxClients);

using tWiFiClientPtr = std::shared_ptr<WiFiClient>;
LinkedList<tWiFiClientPtr> clients;

tN2kDataToNMEA0183 tN2kDataToNMEA0183(&NMEA2000, 0);

// Set the information for other bus devices, which messages we support
const unsigned long TransmitMessages[] PROGMEM = {
  127489UL, // Engine dynamic
  0
};

const unsigned long ReceiveMessages[] PROGMEM = {
  127250UL, // Heading
  127258UL, // Magnetic variation
  128259UL, // Boat speed
  128267UL, // Depth
  129025UL, // Position
  129026UL, // COG and SOG
  129029UL, // GNSS
  130306UL, // Wind
  128275UL, // Log
  127245UL, // Rudder
  0
};

// Forward declarations
void SendNMEA0183Message(const tNMEA0183Msg &NMEA0183Msg);

// Data wire for teperature (Dallas DS18B20) is connected to GPIO 13 on the ESP32
#define ONE_WIRE_BUS 13
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

WebServer web_server(80);   // Web server on TCP port 80

// Battery voltage is connected to GPIO 34 (Analog ADC1_CH6)
const int ADCpin = 34;
float Voltage = 0;
float FridgeTemp = 0;

// Task handles for OneWire and POW read (Core 0 on ESP32)
TaskHandle_t Task1, Task2;

// Serial port 2 config (GPIO 16)
const int baudrate = 38400;
const int rs_config = SERIAL_8N1;

// Buffer config
#define MAX_NMEA0183_MESSAGE_SIZE 150 // For AIS
char buff[MAX_NMEA0183_MESSAGE_SIZE];

// NMEA message for AIS receiving and multiplexing
tNMEA0183Msg NMEA0183Msg;
tNMEA0183 NMEA0183;


void debug_log(char* str) {
#if ENABLE_DEBUG_LOG == 1
  Serial.println(str);
#endif
}


//****************************************************************************************
void setup() {

  uint8_t chipid[6];
  uint32_t id = 0;
  int i = 0;
  int wifi_retry = 0;

  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  button.attachClick(clickedIt);

  Power.Total = 0.003;                // Power on Sonoff starts with this value. Why?

  // Init USB serial port
  Serial.begin(115200);

  // Init AIS serial port 2
  Serial2.begin(baudrate, rs_config);
  NMEA0183.Begin(&Serial2, 3, baudrate);

  if (WLAN_CLIENT == 1) {
    Serial.println("Start WLAN Client");         // WiFi Mode Client

    WiFi.config(CL_local_ip, CL_gateway, CL_subnet, CL_gateway);
    delay(100);
    WiFi.begin(CL_ssid, CL_password);

    while (WiFi.status() != WL_CONNECTED  && wifi_retry < 20) {         // Check connection, try 10 seconds
      wifi_retry++;
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi client connected");
    Serial.println("IP client address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    // Init wifi AP
    Serial.println("Start WLAN AP");     // WiFi Mode AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_ssid, AP_password);
    delay(100);
    WiFi.softAPConfig(AP_local_ip, AP_gateway, AP_subnet);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    wifiType = 1;
  }

  // Start OTA
  ArduinoOTA.setHostname("Gateway");     // Arduino OTA config and start
  ArduinoOTA.begin();

  // Start OneWire
  sensors.begin();

  // Start TCP server
  server.begin();

  // Define web server events and start server
  web_server.on("/", handleRoot);           // This is the Main display page
  web_server.on("/nmea", handleNMEA);       // This is the NMEA display page
  web_server.on("/power", handlePower);     // This is the Power display page
  web_server.on("/heater", handleHeater);   // This is the Heater display page
  web_server.on("/aws", handleAWS);         // Big AWS display page
  web_server.on("/get_data", GetNmeaData);  // To get update of NMEA values
  web_server.on("/get_pow", GetPowerData);  // To get update of Power values
  web_server.on("/p_on", PowerAlarmOn);     // Power alam on
  web_server.on("/p_off", PowerAlarmOff);   // Power alarm off
  web_server.on("/p_reset", PowerReset);    // Reset total energy
  web_server.on("/h_temp", GetHeaterData);  // To get room temperature
  web_server.on("/h_up", HeaterUp);         // Heater + 0.1
  web_server.on("/h_down", HeaterDown);     // Heater - 0.1
  web_server.on("/h_auto", HeaterAuto);     // Heater Auto state
  web_server.on("/h_on", HeaterOn);         // Switch heater On
  web_server.on("/h_off", HeaterOff);       // Switch heater Off
  web_server.on("/h_slider", HeaterSlider); // Handle heater slider move

  web_server.onNotFound(handleNotFound);

  web_server.begin();                       // Start web server
  Serial.println("HTTP server started");

  // Start MDNS
  MDNS.begin("gateway");                 // Start Bonjour protocol (mDNS)
  MDNS.addService("http", "tcp", 80);

  // Reserve enough buffer for sending all messages. This does not work on small memory devices like Uno or Mega

  NMEA2000.SetN2kCANMsgBufSize(8);
  NMEA2000.SetN2kCANReceiveFrameBufSize(150);
  NMEA2000.SetN2kCANSendFrameBufSize(150);

  esp_efuse_read_mac(chipid);
  for (i = 0; i < 6; i++) id += (chipid[i] << (7 * i));

  // Set product information
  NMEA2000.SetProductInformation("1", // Manufacturer's Model serial code
                                 100, // Manufacturer's product code
                                 "NMEA2000 WiFi-Gateway", // Manufacturer's Model ID
                                 "2.0 (2021-12-23)",      // Manufacturer's Software version code
                                 "1.0 (2021-12-23)"       // Manufacturer's Model version
                                );
  // Set device information
  NMEA2000.SetDeviceInformation(id,   // Unique number. Use e.g. Serial number. Id is generated from MAC-Address
                                130,  // Device function=Analog to NMEA 2000 Gateway. See codes on http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                25,   // Device class=Inter/Intranetwork Device. See codes on  http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                2046  // Just choosen free from code list on http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
                               );

  // If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below

  NMEA2000.SetForwardType(tNMEA2000::fwdt_Text); // Show in clear text. Leave uncommented for default Actisense format.

  preferences.begin("nvs", false);                          // Open nonvolatile storage (nvs)
  NodeAddress = preferences.getInt("LastNodeAddress", 32);  // Read stored last NodeAddress, default 32
  TempLevel = preferences.getFloat("TempLevel", 21);
  Auto = preferences.getBool("Auto", false);
  preferences.end();

  Serial.printf("NodeAddress=%d\n", NodeAddress);

  NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, NodeAddress);

  NMEA2000.ExtendTransmitMessages(TransmitMessages);
  NMEA2000.ExtendReceiveMessages(ReceiveMessages);
  NMEA2000.AttachMsgHandler(&tN2kDataToNMEA0183); // NMEA 2000 -> NMEA 0183 conversion
  tN2kDataToNMEA0183.SetSendNMEA0183MessageCallback(SendNMEA0183Message);

  // Create task for core 0, loop() runs on core 1
  xTaskCreatePinnedToCore(
    GetTemperature, /* Function to implement the task */
    "Task1", /* Name of the task */
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    0,  /* Priority of the task */
    &Task1,  /* Task handle. */
    0); /* Core where the task should run */

  // Create task for core 0, loop() runs on core 1
  xTaskCreatePinnedToCore(
    GetTasmota, /* Function to implement the task */
    "Task2", /* Name of the task */
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    1,  /* Priority of the task */
    &Task2,  /* Task handle. */
    0); /* Core where the task should run */

  NMEA2000.Open();
}


//****************************************************************************************
// This task runs isolated on core 0 because sensors.requestTemperatures() is slow and blocking for about 750 ms
void GetTemperature( void * parameter) {
  static float FTO = 0, RTO = 0;
  float tmp = 0;

  for (;;) {
    vTaskDelay(500 / portTICK_PERIOD_MS);  // Block task for 500 ms. Gives more execution time for other tasks
    sensors.requestTemperatures();         // Send the command to get temperatures
    vTaskDelay(100);

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
  }
}


//****************************************************************************************
// This task runs isolated on core 0 because Get_JSON() might block due to unavailabilty of Tasmota POW
void GetTasmota( void * parameter) {
  for (;;) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);    // Block task for 1000 ms. Gives more execution time for other tasks
    Get_JSON_Data();
  }
}


//****************************************************************************************
// Switch Tasmota Heater device On/Off
void  SwitchTasmota(bool OnOff) {
  char command[50];

  HTTPClient http;                      // Generate http client and send command

  if (OnOff) snprintf(command, sizeof(command), "http://%s/cm?cmnd=Power%%20On", TasmotaIP);
  else snprintf(command, sizeof(command), "http://%s/cm?cmnd=Power%%20Off", TasmotaIP);

  http.begin(command);
  http.setConnectTimeout(300);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) Error = false; else Error = true;
  http.end();
}


//****************************************************************************************
// Get data from Tasmota POW
void Get_JSON_Data() {
  char command[50];

  // Allocate JsonBuffer
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<500> root;

  HTTPClient http;
  snprintf(command, sizeof(command), "http://%s/cm?cmnd=status%%208", POW_IP);
  http.begin(command);
  http.setConnectTimeout(300);

  int httpCode = http.GET();

  // httpCode will be negative on error
  if (httpCode > 0) {
    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();

      // Parse JSON object

      DeserializationError error = deserializeJson(root, payload);

      if (error) {
        //Serial.println(F("Parsing failed!"));
        no_connection = 1000;
        http.end();
        return;
      }

      //Serial.println(F("Parsing sucess!"));
      p_update = true;
      no_connection = 0;
      // Extract values

      Power.Total = root["StatusSNS"]["ENERGY"]["Total"] ;
      Power.Power = root["StatusSNS"]["ENERGY"]["Power"] ;
      Power.Voltage = root["StatusSNS"]["ENERGY"]["Voltage"] ;
      Power.Current = root["StatusSNS"]["ENERGY"]["Current"] ;
    }
  } else {
    //Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    no_connection++;
  }
  http.end();
}


//****************************************************************************************
void handleRoot() {
  web_server.send(200, "text/html", MAIN_page); //Send web page
}


//****************************************************************************************
void handleNMEA() {
  web_server.send(200, "text/html", NMEA_page); //Send web page
}


//****************************************************************************************
void handlePower() {
  web_server.send(200, "text/html", POWER_page); //Send web page
}


//****************************************************************************************
void handleHeater() {
  web_server.send(200, "text/html", HEATER_page);  // Send Index Website (defined in heater.h)
}


//****************************************************************************************
void handleAWS() {
  web_server.send(200, "text/html", AWS_page);  // Send Index Website (defined in aws.h)
}


//****************************************************************************************
void HeaterUp() {                       // Handle UP button
  TempLevel += 0.1;
  if (TempLevel > 30 ) TempLevel = 30;
  h_update = true;
  web_server.send(200);
}


//****************************************************************************************
void HeaterDown() {                     // Handle Down button
  TempLevel -= 0.1;
  if (TempLevel < 0) TempLevel = 0;
  h_update = true;
  web_server.send(200);
}


//****************************************************************************************
void HeaterAuto() {                      // Handle Auto button
  Auto = true;
  h_update = true;
  web_server.send(200);
}


//****************************************************************************************
void HeaterOn() {                       // Handle On button
  SwitchTasmota(On);
  Auto = false;
  OnOff = On;
  h_update = true;
  web_server.send(200);
}


//****************************************************************************************
void HeaterOff() {                      // Handle Off button
  SwitchTasmota(Off);
  Auto = false;
  OnOff = Off;
  h_update = true;
  web_server.send(200);
}


//****************************************************************************************
void HeaterSlider() {                   // Handle slider event
  Serial.print("Slider:");
  if (web_server.args() > 0) {
    TempLevel = web_server.arg(0).toFloat();
  }
  h_update = true;
  web_server.send(200);
}


//****************************************************************************************
// Send temperature, level and status as JSON string
void GetHeaterData() {
  String Text, State;
  char buf[30];
  StaticJsonDocument<200> root;

  if (Auto) State = "Auto";
  else {
    if (OnOff) State = "On"; else State = "Off";
  }

  if (Error) State = "Error";

  snprintf(buf, sizeof(buf), "%4.1f", RoomTemp );
  root["temp"] = buf;

  snprintf(buf, sizeof(buf), "%4.1f", TempLevel );
  root["level"] = buf;

  root["status"] = State;

  serializeJsonPretty(root, Text);
  web_server.send(200, "text/plain", Text); //Send sensors values to client ajax request
}


//****************************************************************************************
void PowerAlarmOn() {
  p_sharp = true;
  no_connection = 0;
  web_server.send(200);
}


//****************************************************************************************
void PowerAlarmOff() {
  p_sharp = false;
  p_alarm = false;
  no_connection = 0;
  web_server.send(200);
}


//****************************************************************************************
// Reset total energy on Tasmota POW
void PowerReset() {
  char command[50];
  HTTPClient http;

  snprintf(command, sizeof(command), "http://%s/cm?cmnd=EnergyReset%%203%%200", POW_IP);
  http.begin("command");
  http.setConnectTimeout(300);
  http.GET();
  http.end();
  web_server.send(200);
}


//****************************************************************************************
// Send NMEA data to web client
void GetNmeaData() {
  String Text;
  char buf[30];
  StaticJsonDocument<400> root;
  double minutes;
  double degrees;

  degrees = abs(trunc(BoatData.Latitude));
  minutes = abs((BoatData.Latitude - trunc(BoatData.Latitude)) * 60.0);

  if (BoatData.Latitude > 0) {
    snprintf(buf, sizeof(buf), "%02.0f°%06.3f' N", degrees, minutes);
  } else {
    snprintf(buf, sizeof(buf), "%02.0f°%06.3f' S", degrees, minutes);
  }
  if (abs(BoatData.Latitude) > 1000) sprintf(buf, "---");
  root["lat"] = buf;

  degrees = abs(trunc(BoatData.Longitude));
  minutes = abs((BoatData.Longitude - trunc(BoatData.Longitude)) * 60.0);

  if (BoatData.Longitude > 0) {
    snprintf(buf, sizeof(buf), "%03.0f°%06.3f' E", degrees, minutes);
  } else {
    snprintf(buf, sizeof(buf), "%03.0f°%06.3f' W", degrees, minutes);
  }
  if (abs(BoatData.Longitude) > 1000) sprintf(buf, "---");
  root["lon"] = buf;

  snprintf(buf, sizeof(buf), "%4.1f", BoatData.SOG);
  if (abs(BoatData.SOG) > 1000) sprintf(buf, "---");
  root["sog"] = buf;

  snprintf(buf, sizeof(buf), "%4.1f", BoatData.COG);
  if (abs(BoatData.COG) > 1000) sprintf(buf, "---");
  root["cog"] = buf;

  snprintf(buf, sizeof(buf), "%4.1f", BoatData.Heading);
  if (abs(BoatData.Heading) > 1000) sprintf(buf, "---");
  root["hdg"] = buf;

  snprintf(buf, sizeof(buf), "%4.1f", BoatData.STW);
  root["stw"] = buf;

  snprintf(buf, sizeof(buf), "%4.1f", BoatData.AWS);
  if (abs(BoatData.AWS) > 1000) sprintf(buf, "---");
  root["aws"] = buf;

  snprintf(buf, sizeof(buf), "%4.1f", BoatData.MaxAws);
  root["maxaws"] = buf;

  snprintf(buf, sizeof(buf), "%4.1f", BoatData.WaterDepth);
  root["depth"] = buf;

  snprintf(buf, sizeof(buf), "%4.1f", BoatData.TripLog);
  root["log"] = buf;

  serializeJsonPretty(root, Text);
  web_server.send(200, "text/plain", Text); //Send values to client ajax request
}


//****************************************************************************************
// Send Power data to web client
void GetPowerData(void) {

  String Text;
  char buf[30];
  StaticJsonDocument<300> root;

  snprintf(buf, sizeof(buf), "%7.0f", Power.Voltage);
  root["voltage"] = buf;

  snprintf(buf, sizeof(buf), "%7.1f", Power.Current);
  root["current"] = buf;

  snprintf(buf, sizeof(buf), "%7.0f", Power.Power);
  root["power"] = //buf;

  snprintf(buf, sizeof(buf), "%7.2f", Power.Total);
  root["total"] = buf;

  if (p_sharp) {
    snprintf(buf, sizeof(buf), "On (%d)", no_connection);
    root["alarm"] = buf;
  }
  else root["alarm"] = "Off";

  snprintf(buf, sizeof(buf), "%4.1f", FridgeTemp);
  root["temp"] = buf;

  snprintf(buf, sizeof(buf), "%4.1f", Voltage);
  root["volt"] = buf;

  serializeJsonPretty(root, Text);
  web_server.send(200, "text/plain", Text); //Send values to client ajax request
}


//****************************************************************************************
void handleNotFound() {                                           // Unknown request. Send error 404
  web_server.send(404, "text/plain", "File Not Found\n\n");
}


//****************************************************************************************
void SendBufToClients(const char *buf) {
  for (auto it = clients.begin() ; it != clients.end(); it++) {
    if ( (*it) != NULL && (*it)->connected() ) {
      (*it)->println(buf);
    }
  }
}


//****************************************************************************************
void SendNMEA0183Message(const tNMEA0183Msg & NMEA0183Msg) {

  if ( !SendNMEA0183Conversion ) return;

  char buf[MAX_NMEA0183_MESSAGE_SIZE];
  if ( !NMEA0183Msg.GetMessage(buf, MAX_NMEA0183_MESSAGE_SIZE) ) return;
  SendBufToClients(buf);
}


//****************************************************************************************
bool IsTimeToUpdate(unsigned long NextUpdate) {
  return (NextUpdate < millis());
}


//****************************************************************************************
unsigned long InitNextUpdate(unsigned long Period, unsigned long Offset = 0) {
  return millis() + Period + Offset;
}


//****************************************************************************************
void SetNextUpdate(unsigned long & NextUpdate, unsigned long Period) {
  while ( NextUpdate < millis() ) NextUpdate += Period;
}


//****************************************************************************************
void SendN2kEngine() {
  static unsigned long SlowDataUpdated = InitNextUpdate(SlowDataUpdatePeriod, MiscSendOffset);
  tN2kMsg N2kMsg;

  if ( IsTimeToUpdate(SlowDataUpdated) ) {
    SetNextUpdate(SlowDataUpdated, SlowDataUpdatePeriod);

    SetN2kEngineDynamicParam(N2kMsg, 0, N2kDoubleNA, N2kDoubleNA, CToKelvin(FridgeTemp), Voltage, N2kDoubleNA, N2kDoubleNA, N2kDoubleNA, N2kDoubleNA, N2kInt8NA, N2kInt8NA, true);
    NMEA2000.SendMsg(N2kMsg);
  }
}


//****************************************************************************************
void AddClient(WiFiClient & client) {
  Serial.println("New Client.");
  clients.push_back(tWiFiClientPtr(new WiFiClient(client)));
}


//****************************************************************************************
void StopClient(LinkedList<tWiFiClientPtr>::iterator & it) {
  Serial.println("Client Disconnected.");
  (*it)->stop();
  it = clients.erase(it);
}


//****************************************************************************************
void CheckConnections() {
  WiFiClient client = server.available();   // listen for incoming clients

  if ( client ) AddClient(client);

  for (auto it = clients.begin(); it != clients.end(); it++) {
    if ( (*it) != NULL ) {
      if ( !(*it)->connected() ) {
        StopClient(it);
      } else {
        if ( (*it)->available() ) {
          char c = (*it)->read();
          if ( c == 0x03 ) StopClient(it); // Close connection by ctrl-c
        }
      }
    } else {
      it = clients.erase(it); // Should have been erased by StopClient
    }
  }
}


//****************************************************************************************
// ReadVoltage is used to improve the linearity of the ESP32 ADC see: https://github.com/G6EJD/ESP32-ADC-Accuracy-Improvement-function
double ReadVoltage(byte pin) {
  double reading = analogRead(pin); // Reference voltage is 3v3 so maximum reading is 3v3 = 4095 in range 0 to 4095
  if (reading < 1 || reading > 4095) return 0;
  // return -0.000000000009824 * pow(reading,3) + 0.000000016557283 * pow(reading,2) + 0.000854596860691 * reading + 0.065440348345433;
  return (-0.000000000000016 * pow(reading, 4) + 0.000000000118171 * pow(reading, 3) - 0.000000301211691 * pow(reading, 2) + 0.001109019271794 * reading + 0.034143524634089) * 1000;
} // Added an improved polynomial, use either, comment out as required


//****************************************************************************************
void clickedIt() {  // Button was pressed
  acknowledge = !acknowledge;
}


//****************************************************************************************
void CheckWiFi(void) {
  int wifi_retry;

  if (wifiType == 0) {                                          // Check connection if working as client
    wifi_retry = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_retry < 5 ) {  // Connection lost, 5 tries to reconnect
      wifi_retry++;
      Serial.println("WiFi not connected. Try to reconnect");
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      WiFi.mode(WIFI_STA);
      WiFi.begin(CL_ssid, CL_password);
      delay(100);
    }
    if (wifi_retry >= 5) {
      Serial.println("\nReboot");                                  // Did not work -> restart ESP32
      ESP.restart();
    }
  }
}


//****************************************************************************************
void CheckSourceAddressChange(void) {
  int SourceAddress = NMEA2000.GetN2kSource();

  if (SourceAddress != NodeAddress) { // Save potentially changed Source Address to NVS memory
    NodeAddress = SourceAddress;      // Set new Node Address (to save only once)
    preferences.begin("nvs", false);
    preferences.putInt("LastNodeAddress", SourceAddress);
    preferences.end();
    Serial.printf("Address Change: New Address=%d\n", SourceAddress);
  }
}


//****************************************************************************************
// Handle power alarm
void handlePowerAlarm() {

  if ((no_connection > 5) && (p_sharp == true)) { // Power off alarm
    p_alarm = true;
  }

  if (p_update && (no_connection == 0)) {     // Received new data via JSON from Sonoff POW
    p_alarm = false; // Alarm off
    p_update = false;
  }
}


//****************************************************************************************
// Handle heater
void handleHeaterControl() {

  if (millis() > t + 2000) {    // Every two seconds store changed values and control temperature
    t = millis();

    if (h_update) {             // Store values if changed
      h_update = false;

      preferences.begin("nvs", false);
      preferences.putFloat("TempLevel", TempLevel);
      preferences.putBool("Auto", Auto);
      preferences.end();
    }

    // Serial.print("Temp = ");
    // Serial.println(RoomTemp);

    if (Auto == true ) {           // Switch On/Off depending on temperature

      if ((OnOff == Off) && (RoomTemp < TempLevel)) {
        SwitchTasmota(On);
        OnOff = On;
      }

      if ((OnOff == On) && (RoomTemp > TempLevel + Hyst)) {
        SwitchTasmota(Off);
        OnOff = Off;
      }
    }
  }

  if ((millis() > t2 + 30000)) {  // Every 30 seconds switch On/Off again
    t2 = millis();

    if (Auto == true ) {

      if (RoomTemp < TempLevel) {
        SwitchTasmota(On);
        OnOff = On;
      }

      if (RoomTemp > TempLevel + Hyst) {
        SwitchTasmota(Off);
        OnOff = Off;
      }
    }
  }
}


//****************************************************************************************
void loop() {

  if (NMEA0183.GetMessage(NMEA0183Msg)) {  // Get AIS NMEA sentences from serial2

    SendNMEA0183Message(NMEA0183Msg);      // Send to TCP clients
    NMEA0183Msg.GetMessage(buff, MAX_NMEA0183_MESSAGE_SIZE); // send to buffer

#if ENABLE_DEBUG_LOG == 1
    Serial.println(buff);
#endif

#if UDP_Forwarding == 1
    unsigned int size = strlen(buff);
    udp.beginPacket(udpAddress, udpPort);  // Send to UDP
    udp.write((byte*)buff, size);
    udp.endPacket();
#endif
  }

  Voltage = ((Voltage * 15) + (ReadVoltage(ADCpin) * ADC_Calibration_Value / 4096)) / 16; // This implements a low pass filter to eliminate spike for ADC readings

  // Do updates
  NMEA2000.ParseMessages();
  SendN2kEngine();
  CheckSourceAddressChange();
  tN2kDataToNMEA0183.Update(&BoatData);
  CheckWiFi();
  CheckConnections();
  web_server.handleClient();
  ArduinoOTA.handle();
  handlePowerAlarm();
  handleHeaterControl();
  button.tick();

#if ENABLE_DEBUG_LOG == 2
  Serial.print("Voltage: " ); Serial.println(Voltage);
  //Serial.print("Temperature: ");Serial.println(FridgeTemp);
  Serial.println("");
#endif

  // Alarm handling

  alarmstate = false;
  if ((FridgeTemp > HighTempAlarm) || (Voltage < LowVoltageAlarm) || p_alarm) {
    alarmstate = true;
  }

  if ((alarmstate == true) && (acknowledge == false)) {
    digitalWrite(buzzerPin, HIGH);
  } else {
    digitalWrite(buzzerPin, LOW);
  }

  // Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
  if ( Serial.available() ) {
    Serial.read();
  }
}

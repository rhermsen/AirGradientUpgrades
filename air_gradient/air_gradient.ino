/*
Important: This code is only for the DIY PRO PCB Version 3.7 that has a push button mounted.

This is the code for the AirGradient DIY PRO Air Quality Sensor with an ESP8266 Microcontroller with the SGP40 TVOC module from AirGradient.

It is a high quality sensor showing PM2.5, CO2, Temperature and Humidity on a small display and can send data over Wifi.

Build Instructions: https://www.airgradient.com/open-airgradient/instructions/diy-pro-v37/

Kits (including a pre-soldered version) are available: https://www.airgradient.com/open-airgradient/kits/

The codes needs the following libraries installed:
“WifiManager by tzapu, tablatronix” tested with version 2.0.11-beta
“U8g2” by oliver tested with version 2.32.15
"Sensirion I2C SGP41" by Sensation Version 0.1.0
"Sensirion Gas Index Algorithm" by Sensation Version 3.2.1

Configuration:
Please set in the code below the configuration parameters.

If you have any questions please visit our forum at https://forum.airgradient.com/

If you are a school or university contact us for a free trial on the AirGradient platform.
https://www.airgradient.com/

CC BY-SA 4.0 Attribution-ShareAlike 4.0 International License

*/

// Added by Themi
#include <ESP8266WebServer.h>

#include <AirGradient.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include <EEPROM.h>

//#include "SGP30.h"
#include <SensirionI2CSgp41.h>
#include <NOxGasIndexAlgorithm.h>
#include <VOCGasIndexAlgorithm.h>


#include <U8g2lib.h>

AirGradient ag = AirGradient();
SensirionI2CSgp41 sgp41;
VOCGasIndexAlgorithm voc_algorithm;
NOxGasIndexAlgorithm nox_algorithm;
// time in seconds needed for NOx conditioning
uint16_t conditioning_s = 10;

// for persistent saving and loading
int addr = 4;
byte value;

// Display bottom right
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Replace above if you have display on top left
//U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);


// CONFIGURATION START

// Added by Themi
// Set to true if you'd like to report to an airgradient endpoint
boolean doReport = true;
// Http port number
const int port = 8080;
// Prometheus device id
const String deviceId = "AirGradient1";

//set to the endpoint you would like to use
String APIROOT = "http://hw.airgradient.com/";

// set to true to switch from Celcius to Fahrenheit
boolean inF = false;

// PM2.5 in US AQI (default ug/m3)
boolean inUSAQI = false;

// Display Position
boolean displayTop = true;

// set to true if you want to connect to wifi. You have 90 seconds to connect. Then it will go into an offline mode.
boolean connectWIFI=true;

// CONFIGURATION END

// Added by Themi
ESP8266WebServer server(port);

unsigned long currentMillis = 0;

const int oledInterval = 5000;
unsigned long previousOled = 0;

const int sendToServerInterval = 10000;
unsigned long previoussendToServer = 0;

const int tvocInterval = 1000;
unsigned long previousTVOC = 0;
int TVOC = 0;
int NOX = 0;

const int co2Interval = 5000;
unsigned long previousCo2 = 0;
int Co2 = 0;

//const int pm25Interval = 5000;
const int pm25Interval = 45000;
const int pm25SleepIntvl = 150000;
unsigned long previousPm25 = 0;
//unsigned long previousSleepPm25 = 0;
int pm25 = 0;
int pm25Old = 0;
int pm25Failed = 0;
boolean pm25sleep = false;

const int tempHumInterval = 2500;
unsigned long previousTempHum = 0;
float temp = 0;
int hum = 0;

int buttonConfig=4;
int lastState = LOW;
int currentState;
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Hello");
  u8g2.begin();
  //u8g2.setDisplayRotation(U8G2_R0);

  EEPROM.begin(512);
  delay(500);

  buttonConfig = String(EEPROM.read(addr)).toInt();
  setConfig();

   updateOLED2("Press Button", "Now for", "Config Menu");
    delay(2000);

  currentState = digitalRead(D7);
  if (currentState == HIGH)
  {
    updateOLED2("Entering", "Config Menu", "");
    delay(3000);
    lastState = LOW;
    inConf();
  }

  if (connectWIFI)
  {
     connectToWifi();
  }

  updateOLED2("Warming Up", "Serial Number:", String(ESP.getChipId(), HEX));
  sgp41.begin(Wire);
  ag.CO2_Init();
  ag.PMS_Init();
  ag.TMP_RH_Init(0x44);

  // Added by Themi
  server.on("/metrics", HandleMetrics);
  server.on("/metricsjson", HandleJsonMetrics);
  server.on("/", HandleIndexHTML);
  server.onNotFound(HandleNotFound);
  server.begin();
}

// Added by Themi
void HandleMetrics() {
  String message = "";
  message += GetPrometheusString("pm02 Particulate Matter PM2.5 value", "pm02 gauge", "pm02",  "µg/m³", String(pm25) );
  message += GetPrometheusString("rco2 CO2 value, in ppm", "rco2 gauge", "rco2", "ppm", String(Co2) );
  message += GetPrometheusString("Temperature, in degrees Celcius", "temp gauge", "temp", "°C", String(temp) );
  message += GetPrometheusString("rhum Relative humidity, in percent", "rhum gauge", "rhum", "%", String(hum) );
  message += GetPrometheusString("TVOC index value", "tvoc gauge", "tvoc", "index", String(TVOC) );
  message += GetPrometheusString("NOX index value", "nox gauge", "nox", "index", String(NOX) );
  message += GetPrometheusString("PM2.5 Read failed count", "rfail counter", "rfail", "count", String(pm25Failed) );
  server.send(200, "text/plain", message);
}

void HandleJsonMetrics() {
  String message = "{\"pm02\":"+ String(pm25) + ",\"rco2\":" + String(Co2) + ",\"temp\":" + String(temp) + ",\"rhum\":" + String(hum) + ",\"tvoc\":" + String(TVOC) + ",\"nox\":" + String(NOX) + ",\"rfail\":" + String(pm25Failed) + "}";
  server.send(200, "application/json", message);
}

void HandleIndexHTML() {
  String message = "<!doctype html>";
  message += "<title>CO2-Monitor</title>";
  message += "<section class=\"content\">";
  message += "  <h1 style=\"font-size: 24.47px\">Welcome to CO2-Monitor</h1>";
  message += "  <p>I'm a C++ script to received sensor data from the CO2-Monitor and make it available via HTTP</p>";
  message += "  <p>For prometheus metrics, see <a href=\"/metrics\">/metrics</a></p>";
  message += "  <p>For the raw JSON data try <a href=\"/metricsjson\">/metricsjson</a></p>";
  server.send(200, "text/html", message);
}

// Added by Themi
String GetPrometheusString(String proHelp, String proType, String proMetric, String proUnit, String proStat) {
  String idString = "{id=\"" + String(deviceId) + "\",name=\"" + proType + "\",unit=\"" + proUnit + "\"}";
  String proOut = "";
  proOut += "# HELP " + proHelp + "\n";
  proOut += "# TYPE " + proType + "\n";
  proOut += proMetric;
  proOut += idString;
  proOut += proStat;
  proOut += "\n";
  return proOut;
}

// Added by Themi
void HandleNotFound() {
  server.send(404, "text/plain", "Not found");  
}

void loop() {
  currentMillis = millis();
  updateTVOC();
  updateOLED();
  updateCo2();
  //
  sleepPm25();
  updatePm25();
  updateTempHum();
  // Added by Themi
  if (doReport) {
  sendToServer();
}
  server.handleClient();
}

void inConf(){
  setConfig();
  currentState = digitalRead(D7);

  if(lastState == LOW && currentState == HIGH) {
    pressedTime = millis();
  }

  else if(lastState == HIGH && currentState == LOW) {
    releasedTime = millis();
    long pressDuration = releasedTime - pressedTime;
    if( pressDuration < 1000 ) {
      buttonConfig=buttonConfig+1;
      if (buttonConfig>7) buttonConfig=0;
    }
  }

  if (lastState == HIGH && currentState == HIGH){
     long passedDuration = millis() - pressedTime;
      if( passedDuration > 4000 ) {
        // to do
//        if (buttonConfig==4) {
//          updateOLED2("Saved", "Release", "Button Now");
//          delay(1000);
//          updateOLED2("Starting", "CO2", "Calibration");
//          delay(1000);
//          Co2Calibration();
//       } else {
          updateOLED2("Saved", "Release", "Button Now");
          delay(1000);
          updateOLED2("Rebooting", "in", "5 seconds");
          delay(5000);
          EEPROM.write(addr, char(buttonConfig));
          EEPROM.commit();
          delay(1000);
          ESP.restart();
 //       }
    }

  }
  lastState = currentState;
  delay(100);
  inConf();
}


void setConfig() {
  if (buttonConfig == 0) {
    updateOLED2("Temp. in C", "PM in ug/m3", "Display Top");
      u8g2.setDisplayRotation(U8G2_R2);
      inF = false;
      inUSAQI = false;
  }
    if (buttonConfig == 1) {
    updateOLED2("Temp. in C", "PM in US AQI", "Display Top");
      u8g2.setDisplayRotation(U8G2_R2);
      inF = false;
      inUSAQI = true;
  }
   if (buttonConfig == 2) {
    updateOLED2("Temp. in F", "PM in ug/m3", "Display Top");
      u8g2.setDisplayRotation(U8G2_R2);
      inF = true;
      inUSAQI = false;
  }
   if (buttonConfig == 3) {
    updateOLED2("Temp. in F", "PM in US AQI", "Display Top");
      u8g2.setDisplayRotation(U8G2_R2);
       inF = true;
      inUSAQI = true;
  }
    if (buttonConfig == 4) {
    updateOLED2("Temp. in C", "PM in ug/m3", "Display Bottom");
      u8g2.setDisplayRotation(U8G2_R0);
      inF = false;
      inUSAQI = false;
  }
    if (buttonConfig == 5) {
    updateOLED2("Temp. in C", "PM in US AQI", "Display Bottom");
      u8g2.setDisplayRotation(U8G2_R0);
      inF = false;
      inUSAQI = true;
  }
   if (buttonConfig == 6) {
    updateOLED2("Temp. in F", "PM in ug/m3", "Display Bottom");
    u8g2.setDisplayRotation(U8G2_R0);
      inF = true;
      inUSAQI = false;
  }
   if (buttonConfig == 7) {
    updateOLED2("Temp. in F", "PM in US AQI", "Display Bottom");
      u8g2.setDisplayRotation(U8G2_R0);
       inF = true;
      inUSAQI = true;
  }



  // to do
  // if (buttonConfig == 8) {
  //  updateOLED2("CO2", "Manual", "Calibration");
  // }
}

void updateTVOC()
{
 uint16_t error;
    char errorMessage[256];
    uint16_t defaultRh = 0x8000;
    uint16_t defaultT = 0x6666;
    uint16_t srawVoc = 0;
    uint16_t srawNox = 0;
    uint16_t defaultCompenstaionRh = 0x8000;  // in ticks as defined by SGP41
    uint16_t defaultCompenstaionT = 0x6666;   // in ticks as defined by SGP41
    uint16_t compensationRh = 0;              // in ticks as defined by SGP41
    uint16_t compensationT = 0;               // in ticks as defined by SGP41

    delay(1000);

    compensationT = static_cast<uint16_t>((temp + 45) * 65535 / 175);
    compensationRh = static_cast<uint16_t>(hum * 65535 / 100);

    if (conditioning_s > 0) {
        error = sgp41.executeConditioning(compensationRh, compensationT, srawVoc);
        conditioning_s--;
    } else {
        error = sgp41.measureRawSignals(compensationRh, compensationT, srawVoc,
                                        srawNox);
    }

    if (currentMillis - previousTVOC >= tvocInterval) {
      previousTVOC += tvocInterval;
      TVOC = voc_algorithm.process(srawVoc);
      NOX = nox_algorithm.process(srawNox);
      Serial.println(String(TVOC));
    }
}

void updateCo2()
{
    if (currentMillis - previousCo2 >= co2Interval) {
      previousCo2 += co2Interval;
      Co2 = ag.getCO2_Raw();
      Serial.println(String(Co2));
    }
}

void sleepPm25()
{
    if (currentMillis - previousPm25 >= pm25SleepIntvl && pm25sleep) {
// This will not work because it is true also 
//      previousSleepPm25 = currentMillis;
      ag.wakeUp();
      pm25sleep = false;
    }
}

void updatePm25()
{
    if (currentMillis - previousPm25 >= pm25SleepIntvl + pm25Interval) {
      previousPm25 = currentMillis;
//    if (currentMillis - previousPm25 >= pm25Interval) {
//      previousPm25 = currentMillis;
      pm25Old = ag.getPM2_Raw();
      if (pm25Old >= 0) {
        pm25 = pm25Old;
      } else {
        pm25Failed++;
      }
      Serial.println(String(pm25));
      delay(1000);
      pm25sleep = true;
      ag.sleep();
    }
}

void updateTempHum()
{
    if (currentMillis - previousTempHum >= tempHumInterval) {
      previousTempHum += tempHumInterval;
      TMP_RH result = ag.periodicFetchData();
      temp = result.t;
      hum = result.rh;
      Serial.println(String(temp));
    }
}

void updateOLED() {
   if (currentMillis - previousOled >= oledInterval) {
     previousOled += oledInterval;

    String ln3;
    String ln1;

    if (inUSAQI) {
      ln1 = "AQI:" + String(PM_TO_AQI_US(pm25)) +  " CO2:" + String(Co2);
    } else {
      ln1 = "PM:" + String(pm25) +  " CO2:" + String(Co2);
    }

     String ln2 = "TVOC:" + String(TVOC) + " NOX:" + String(NOX);

      if (inF) {
        ln3 = "F:" + String((temp* 9 / 5) + 32) + " H:" + String(hum)+"%";
        } else {
        ln3 = "C:" + String(temp) + " H:" + String(hum)+"%";
       }
     updateOLED2(ln1, ln2, ln3);
   }
}

void updateOLED2(String ln1, String ln2, String ln3) {
  char buf[9];
  u8g2.firstPage();
  u8g2.firstPage();
  do {
  u8g2.setFont(u8g2_font_t0_16_tf);
  u8g2.drawStr(1, 10, String(ln1).c_str());
  u8g2.drawStr(1, 30, String(ln2).c_str());
  u8g2.drawStr(1, 50, String(ln3).c_str());
    } while ( u8g2.nextPage() );
}

void sendToServer() {
   if (currentMillis - previoussendToServer >= sendToServerInterval) {
     previoussendToServer += sendToServerInterval;
      String payload = "{\"wifi\":" + String(WiFi.RSSI())
      + (Co2 < 0 ? "" : ", \"rco2\":" + String(Co2))
      + (pm25 < 0 ? "" : ", \"pm02\":" + String(pm25))
      + (TVOC < 0 ? "" : ", \"tvoc_index\":" + String(TVOC))
      + (NOX < 0 ? "" : ", \"nox_index\":" + String(NOX))
      + ", \"atmp\":" + String(temp)
      + (hum < 0 ? "" : ", \"rhum\":" + String(hum))
      + "}";

      if(WiFi.status()== WL_CONNECTED){
        Serial.println(payload);
        String POSTURL = APIROOT + "sensors/airgradient:" + String(ESP.getChipId(), HEX) + "/measures";
        Serial.println(POSTURL);
        WiFiClient client;
        HTTPClient http;
        http.begin(client, POSTURL);
        http.addHeader("content-type", "application/json");
        int httpCode = http.POST(payload);
        String response = http.getString();
        Serial.println(httpCode);
        Serial.println(response);
        http.end();
      }
      else {
        Serial.println("WiFi Disconnected");
      }
   }
}

// Wifi Manager
 void connectToWifi() {
   WiFiManager wifiManager;
   //WiFi.disconnect(); //to delete previous saved hotspot
   String HOTSPOT = "AG-" + String(ESP.getChipId(), HEX);
   updateOLED2("90s to connect", "to Wifi Hotspot", HOTSPOT);
   wifiManager.setTimeout(90);












   if (!wifiManager.autoConnect((const char * ) HOTSPOT.c_str())) {
     updateOLED2("booting into", "offline mode", "");
     Serial.println("failed to connect and hit timeout");
     delay(6000);
   }

}

// Calculate PM2.5 US AQI
int PM_TO_AQI_US(int pm02) {
  if (pm02 <= 12.0) return ((50 - 0) / (12.0 - .0) * (pm02 - .0) + 0);
  else if (pm02 <= 35.4) return ((100 - 50) / (35.4 - 12.0) * (pm02 - 12.0) + 50);
  else if (pm02 <= 55.4) return ((150 - 100) / (55.4 - 35.4) * (pm02 - 35.4) + 100);
  else if (pm02 <= 150.4) return ((200 - 150) / (150.4 - 55.4) * (pm02 - 55.4) + 150);
  else if (pm02 <= 250.4) return ((300 - 200) / (250.4 - 150.4) * (pm02 - 150.4) + 200);
  else if (pm02 <= 350.4) return ((400 - 300) / (350.4 - 250.4) * (pm02 - 250.4) + 300);
  else if (pm02 <= 500.4) return ((500 - 400) / (500.4 - 350.4) * (pm02 - 350.4) + 400);
  else return 500;
};

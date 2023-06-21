## AirGradientUpgrades

**Note: this software is aimed to only the AirGradient Pro 3.7 with TVOC.** You can certainly adapt this script if you don't have the TVOC module in your Air Gradient.

This modification adds a http /metrics (for prometheus), a /metricsjson, and a / endpoint on port 8080.
It also introduces configuration changes to extend lifespan of the PMS5003.
Obtained the best results with waiting 45 seconds before reading from the sensor.

In the latest update `-1` readings from the PMS5003 are ignored. So far I have seen upto 10 `-1` readings per day.
(this might cause degradation of the sensor to become unnoticed)

This script is forked from https://github.com/Themis3000/AirGradientUpgrades, which was taken from https://github.com/airgradienthq/arduino. 


### Setup

Clone this repo and open `air_gradient/air_gradient.ino`. Install all libraries at the correct version listed in the comment. After installing those libraries, additionally install `AirGradient Air Quality Sensor`.

Upgrade ESP8266 to `v3.1.2` to have SoftwareSerial 8.0.1 included.
See findings of ken830 on the AirGradient forum.

Used with:
ESP8266 Arduino library (Core)						v3.1.2
Libraries:
AirGradient Air Quality Sensor						v2.4.3
ESP8266 and ESP32 OLED driver for SSD1306 displays	v4.4.0
Sensirion Core 										v0.6.0
Sensirion Gas Index Algorithm						v3.2.2
Sensirion I2C SGP41									v0.1.0
U8g2												v2.32.15
WiFiManager											v2.0.15-rc.1
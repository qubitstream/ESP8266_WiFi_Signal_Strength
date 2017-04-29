/*
A WiFi signal strength tracker sketch for ESP8266 based devices
Requires a 128x32 OLED display and a potentiometer
 
Copyright (C) 2017 Christoph Haunschmidt

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <Adafruit_SSD1306.h>
#include "wifi_credentials.h"

const int WIDTH = 128;
const int HEIGHT = 32;
const unsigned char ANALOG_PIN = 0,
                    OLED_RESET = LED_BUILTIN;


unsigned char dBmToPercent(long dBm) {
  unsigned char quality_percent = 0;
  if (dBm <= -100)
    quality_percent = 0;
  else if (dBm >= -50)
    quality_percent = 100;
  else
    quality_percent = 2 * (dBm + 100);
  return quality_percent;
}


class RSSIData {
  private:
    long current_rssi = 0;
    unsigned int data_points_count = 0,
                 data_points_available = 0,
                 data_points_position = 0,
                 update_every_ms = 100;
    unsigned long last_update_ms = 0,
                  next_update_due_ms = millis();
    unsigned char timespan_for_average_secs = 5;
    long* data_points;

  public:
    RSSIData(unsigned int data_points_count, unsigned char seconds_for_average);
    ~RSSIData() {
      delete [] data_points;
    };
    float getAverage();
    bool update(long new_rssi);
    long getCurrent() {
      return current_rssi;
    };
    void setTimespanForAverage(unsigned char seconds) {
      timespan_for_average_secs = seconds;
      update_every_ms = (timespan_for_average_secs * 1000.0) / data_points_count;
    };
    unsigned char getTimespanForAverage() {
      return timespan_for_average_secs;
    };
};

RSSIData::RSSIData(unsigned int data_points_count,
                   unsigned char seconds_for_average): data_points_count(data_points_count) {
  data_points = new long[this->data_points_count];
  setTimespanForAverage(seconds_for_average);
}

float RSSIData::getAverage() {
  if (data_points_available < 1) {
    return 0.0;
  }
  long sum = 0;
  for (unsigned int i = 0; i < data_points_available; i++) {
    sum += data_points[i];
  }
  return float(sum) / data_points_available;
}

bool RSSIData::update(long new_rssi) {
  if (millis() >= next_update_due_ms) {
    current_rssi = new_rssi;
    data_points[data_points_position] = new_rssi;
    data_points_position = (data_points_position + 1) % data_points_count;
    if (data_points_available < data_points_count) {
      data_points_available++;
    }
    next_update_due_ms = update_every_ms + last_update_ms;
    last_update_ms = millis();
    return true;
  }
  return false;
}


Adafruit_SSD1306 display(OLED_RESET);
long rssi;
unsigned char percentCurrent = 0, percentAverage = 0;
RSSIData rssi_data(50, 10);
bool updated = false;


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // initialize with the I2C addr 0x3C (for the 128x32)
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("Connecting to " + String(SSID));
  display.display();
  Serial.print("Connecting to ");
  Serial.print(SSID);
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
  }
  Serial.println("success!");
  display.clearDisplay();
  display.display();
}


void loop() {
  rssi = WiFi.RSSI();
  updated = rssi_data.update(rssi);
  if (updated) {
    digitalWrite(LED_BUILTIN, LOW);  // internal led is active LOW
  }

  percentCurrent = dBmToPercent(rssi);
  percentAverage = dBmToPercent(rssi_data.getAverage());

  Serial.print(String(percentAverage) + "\t");
  Serial.println(String(percentCurrent));

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(String(percentCurrent) + "% | AVG: ");
  display.print(String(percentAverage) + "% (" + String(rssi_data.getTimespanForAverage()) + "s)");
  display.drawFastHLine(0, HEIGHT - int(dBmToPercent(rssi) * HEIGHT / 100.0), WIDTH, WHITE);
  display.display();

  delay(1);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(49);
}


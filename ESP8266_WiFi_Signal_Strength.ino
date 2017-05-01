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
#include <vector>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <Adafruit_SSD1306.h>
#include "wifi_credentials.h"

const int WIDTH = 128;
const int HEIGHT = 32;
const unsigned char ANALOG_PIN = 0,
                    OLED_RESET = LED_BUILTIN;
const unsigned char DATAPOINTS_PER_SECOND = 10;
const unsigned char SELECTABLE_TIMESPANS[] = {1, 3, 5, 10, 30, 60};


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
    long current_rssi_cumulative = 0;
    unsigned int current_rssi_cumulative_data_points_count = 0,
                 data_points_count = 0,
                 data_points_available = 0,
                 data_points_position = 0,
                 update_every_ms = 100;
    unsigned long last_update_ms = 0,
                  next_update_due_ms = millis();
    unsigned char timespan_for_average_secs = 5;
    long* data_points = nullptr;

  public:
    RSSIData(unsigned char seconds_for_average);
    ~RSSIData() {
      if (data_points) delete [] data_points;
    };
    float getAverage();
    bool update(long new_rssi);
    long getCurrent() {
      return current_rssi_cumulative_data_points_count
             ? long(float(current_rssi_cumulative) / current_rssi_cumulative_data_points_count)
             : 0;
    };
    void setTimespanForAverage(unsigned char seconds) {
      timespan_for_average_secs = seconds;
      data_points_count = seconds * DATAPOINTS_PER_SECOND;
      if (data_points) delete [] data_points;
      data_points = new long[data_points_count];
      data_points_position = 0;
      data_points_available = 0;
    };
    unsigned char getTimespanForAverage() {
      return timespan_for_average_secs;
    };
};

RSSIData::RSSIData(unsigned char seconds_for_average) {
  update_every_ms = 1000.0 / DATAPOINTS_PER_SECOND;
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
  current_rssi_cumulative += new_rssi;
  current_rssi_cumulative_data_points_count++;
  if (millis() >= next_update_due_ms) {
    current_rssi = getCurrent();
    data_points[data_points_position] = new_rssi;
    data_points_position = (data_points_position + 1) % data_points_count;
    if (data_points_available < data_points_count) {
      data_points_available++;
    }
    next_update_due_ms = update_every_ms + last_update_ms;
    last_update_ms = millis();
    current_rssi_cumulative = 0;
    current_rssi_cumulative_data_points_count = 0;
    return true;
  }
  return false;
}

Adafruit_SSD1306 display(OLED_RESET);
long rssi;
unsigned char percent_current = 0,
              percent_average = 0,
              seconds_for_average = 10,
              analog_value;
RSSIData rssi_data(seconds_for_average);
bool updated = false;


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ANALOG_PIN, INPUT);
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

void drawScale() {
  for (int i = 0; i < 100; i++) {
    if (i % 25 == 0) {
      display.drawFastVLine(int(i * WIDTH / 100.0), HEIGHT - 8, 8, WHITE);
    } else if (i % 10 == 0) {
      display.drawFastVLine(int(i * WIDTH / 100.0), HEIGHT - 2, 2, WHITE);
    }
  }
}

void loop() {
  // leave a bit of space for the bounds
  analog_value = constrain(map(analogRead(ANALOG_PIN), 24, 1000, 0, 5), 0, 5);
  if (analog_value != seconds_for_average) {
    seconds_for_average = analog_value;
    rssi_data.setTimespanForAverage(SELECTABLE_TIMESPANS[seconds_for_average]);
  }

  rssi = WiFi.RSSI();
  updated = rssi_data.update(rssi);

  percent_current = dBmToPercent(rssi);
  percent_average = dBmToPercent(rssi_data.getAverage());

  if (updated) {
    /* instead of pwm, we use the tone function for lower frequencies for blinking the led
      10 Hz is the slowest frequency in my tests (unlike the Arduino UNOs 31Hz) */
    if (percent_average) {
      tone(LED_BUILTIN, map(percent_average, 0, 100, 10, 32));
    } else {
      noTone(LED_BUILTIN);
    }
  }

  Serial.print(String(percent_average) + "\t");
  Serial.println(String(percent_current));

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(String(percent_current) + "% | AVG: ");
  display.print(String(percent_average) + "% (" + String(rssi_data.getTimespanForAverage()) + "s)");

  drawScale();
  display.drawFastVLine(int(WIDTH * percent_current / 100.0), 0, HEIGHT, INVERSE);
  display.fillRect(0, 0, int(WIDTH * percent_average / 100.0), HEIGHT, INVERSE);

  display.display();

  delay(1);
  //digitalWrite(LED_BUILTIN, HIGH);
  delay(49);
}


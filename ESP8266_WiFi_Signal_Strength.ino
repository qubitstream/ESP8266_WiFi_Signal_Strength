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
#include <deque>
#include <numeric>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <Adafruit_SSD1306.h>
#include "wifi_credentials.h"

// Constants
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
                 update_every_ms = 100;
    unsigned long last_update_ms = 0,
                  next_update_due_ms = millis();
    unsigned char timespan_for_average_secs = 5;
    std::deque<long> data_points;

  public:
    RSSIData(unsigned char seconds_for_average);
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
      data_points.clear();
    };
    unsigned char getTimespanForAverage() {
      return timespan_for_average_secs;
    };
    const std::deque<long>& getAverageData() {
      return data_points;
    }
};

RSSIData::RSSIData(unsigned char seconds_for_average) {
  update_every_ms = 1000.0 / DATAPOINTS_PER_SECOND;
  setTimespanForAverage(seconds_for_average);
}

float RSSIData::getAverage() {
  return data_points.size()
         ? std::accumulate(data_points.begin(), data_points.end(), 0) / float(data_points.size())
         : 0.0;
}

bool RSSIData::update(long new_rssi) {
  current_rssi_cumulative += new_rssi;
  current_rssi_cumulative_data_points_count++;
  if (millis() >= next_update_due_ms) {
    current_rssi = getCurrent();
    if (data_points.size() >= data_points_count) {
      data_points.pop_back();
    }
    data_points.push_front(current_rssi);
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
    display.drawFastVLine(WIDTH / 2, HEIGHT / 2 - 3, 6, INVERSE);
    display.display();
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    display.drawFastHLine(WIDTH / 2 - 3, HEIGHT / 2 - 3, 6, INVERSE);
    display.display();
    delay(250);
  }
  Serial.println("success!");
  display.clearDisplay();
  display.display();
}

// a common Bresenham line drawing implementation
void line(int x0, int y0, int x1, int y1, int color) {
  int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2; /* error value e_xy */

  while (1) {
    display.drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    e2 = 2 * err;
    if (e2 > dy) {
      err += dy;  /* e_xy+e_x > 0 */
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;  /* e_xy+e_y < 0 */
      y0 += sy;
    }
  }
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

void drawAverage() {
  const std::deque<long>& data_points = rssi_data.getAverageData();
  const float stepWidth = float(WIDTH) / data_points.size();
  int i = 0, newY, lastY;
  for (const auto& dp : data_points) {
    newY = map(dBmToPercent(dp), 0, 100, HEIGHT - 1, 0);
    if (i > 0) {
      line(int(WIDTH - i * stepWidth), lastY, int(WIDTH - (i + 1) * stepWidth), newY, WHITE);
    }
    lastY = newY;
    i++;
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
    /* instead of PWM, we use the tone function for lower frequencies for blinking the LED
      10 Hz is the lowest frequency possible in my tests for the ESP8266 (lower than the Arduino UNOs 31 Hz) */
    if (percent_average) {
      tone(LED_BUILTIN, map(percent_average, 0, 100, 10, 26));
    } else {
      noTone(LED_BUILTIN);
    }
  }

  Serial.print(String(percent_average) + "\t");
  Serial.println(percent_current);

  display.clearDisplay();

  drawScale();
  drawAverage();
  display.drawFastVLine(int(WIDTH * percent_current / 100.0), 0, HEIGHT, INVERSE);
  display.drawFastHLine(int(WIDTH * percent_current / 100.0) - 2, HEIGHT / 2, 5 , INVERSE);
  display.fillRect(0, 0, int(WIDTH * percent_average / 100.0), HEIGHT, INVERSE);

  display.setCursor(0, 0);
  display.setTextColor(INVERSE);
  display.print(String(percent_current) + "% | AVG: ");
  display.print(String(percent_average) + "% (" + String(rssi_data.getTimespanForAverage()) + "s)");

  display.display();

  delay(40);
}


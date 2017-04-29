ESP8266 WiFi Signal Strength
===========================

## About

This is a sketch for checking the signal strength of WiFi signals.
Both the current signal and the average strength for an adjustable timespan
are displayed.

## Requirements

### Hardware

- an ESP8266 based microcontroller (it has been tested on a WeMos D1 Mini,
  but a NodeMCU or similar devices should work fine as well)
- an SSD1306 based, 128x32 pixel I2C OLED display (it should be easy to adjust for
  a SPI based displays)
- a simple potentiometer

### Software

- Arduino IDE with ESP8266 support
- [Adafruit_SSD1306](https://github.com/adafruit/Adafruit_SSD1306) library

### Adaptions

Change the WiFi credentials in `wifi_credentials.h` to the approbiate values.

## License

GNU General Public License, Version 2

## Authors

Christoph Haunschmidt
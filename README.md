# Cheaperstick

A device consisting of a Wemos d1 mini, a 433MHz receiver and a transmitter.

It can be used to control cheap home control devices. Currently it supports some Prove brand devices, but will be extended to support other devices too.

## Features

- connects to a WiFi access point
- connects to an MQTT server
- sends an MQTT message on receiving a 433MHz code from a remote control
- on receiving an MQTT message it sends a 433MHz remote control code
- configuration is stored in EEPROM

## Planned Features

- USB serial interface as an alternative to WiFi for receiving and sending remote control codes
- REST interface as an alternative to MQTT
- Web based admin page for
  - enabling/disabling WiFi, USB, MQTT, REST
  - setting WiFi ssid and password
  - setting MQTT server ip
  - setting mDNS name to be able to reach admin page by ip name instead of number
  - sniffing various types of remote controls
- acting as an AP if it can't connect to WiFi, to enable access to admin page

## Supported hardware

### Wemos d1 mini

See https://www.wemos.cc/product/d1-mini.html

![d1 mini image](docs/d1mini.jpg)

### Receiver & transmitter

See http://www.ebay.com/itm/433Mhz-RF-transmitter-and-receiver-kit-for-Arduino-ARM-MCU-WL-/261041100836

![receiver transmitter image](docs/rxtx.jpeg)

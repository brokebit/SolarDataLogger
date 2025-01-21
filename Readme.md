![alt text](https://github.com/brokebit/SolarDataLoggerV2-ESP-IDF/blob/main/assets/BoardScreenshot.png?raw=true)

# What is all this

Firmware for custom data logger for Victron MPPT charge controllers via the Ve.Direct port/protocol.

## Features
- Power via 9v-20v input or USB
- ESP32-C6 RISC-V CPU
- Wifi 6 support
- Receive, parse, and publish data from a Victron MPPT controller
- Publishes: panel voltage, battery voltage, panel current, battery current, serial number, etc
- Publish data via MQTT in InfluxDB line protocol format
- Onboard temperature and humidity sensors (AHT20)
- Support for optional add on board with current sensor (INA226) and solid state relay (CMX60D10)
- WS2812 LED for status information
- CP2102N USB Uart for programing and logging
- USB port direct to ESP32-C6


## Supported Victron MPPT Controllers

Supports both the smaller MPPT controllers that have load output and control:

- MPPT 75/10
- MPPT 75/15
- MPPT 100/10
- MPPT 100/20

Also support larger MPPT controllers without load output via a INA226 Current sensor on an add-on board:

- MPPT 100/30
- MPPT 100/50
- MPPT 150/35
- MPPT 150/45
- MPPT 150/60
- MPPT 250/70

## The Details
This is a Platform IO project. Last tested with ESP IDF 5.3.1

- /lib contains drivers and victron parser
- /src contains main program

Edit platform.ini to set some global variables and config parameters. 
- INA226_PRESENT: Set this to one if you have a INA226 connected. Otherwise 0. 
- AHT20_PRESENT: Set this to 1 if you have a AHT20 connected. Otherwise 0.
- SENSOR_LOCATION: Set this to any string you want. It will be included in the mqtt message. No more than 24 characters
- SENSOR_TYPE: Set this to whatever you want. i.e. ESP32-C6 or ESP32-S3. No more than 24 characters
           
![alt text](https://github.com/brokebit/SolarDataLogger/blob/main/hardware/Schematic-v1.1.svg?raw=true)
# What can you do with it? 

## Grafana Dashboard Example
![alt text](https://github.com/brokebit/SolarDataLogger/blob/main/assets/GrafanaScreenshot.png?raw=true)

# To Do
- Fix math for INA226 current sensor and address twos compliment
- CMX60D10 Relay Driver
- OTA Updates
- Some way to configure it without recompiling 
- Redo design in KiCad instead of EasyEDA and add to repo
- Finish add onboard with INA226 and CMX60D10
- Code clean up. Comments, proper error handling, review memory managment, null point checks, etc

# Done
- WS2812 Driver

<p float="left">
  <img src="https://github.com/brokebit/SolarDataLogger/blob/main/assets/Brokebit-Cat-Logo.png?raw=true" width="200" />
  <img src="https://github.com/brokebit/SolarDataLogger/blob/main/assets/oshw-logo-800-px.png?raw=true" width="200" />
</p>

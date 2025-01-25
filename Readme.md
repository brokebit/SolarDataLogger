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
- Remote control via MQTT command channel


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
# MQTT

## Channels
- Influx data is sent to MQTT_TOPIC
- Command should be submitted to the channel named mac address + "-cmd" i.e. FFEEDDCCBBAA-cmd 
- Commands responses are sent to the channel named mac address. i.e. FFEEDDCCBBAA

## Support remote MQTT Commands
- ota - Perform an OTA Update
- rbk - Roll back to previous version 
- rst - Reboot
- png - Get a pong back
- tmp - Get current temp in C
- hum - Get current humidity
- mem - Get free heap memory
- ver - Get firmware version

# OTA
- Firmware is download via https from any URL. The firmware looks for a file that is the mac address in all caps. 
- Firmware URL is defined in : OTA_BASE_URL (without the filename)

# What can you do with it? 

## Grafana Dashboard Example
![alt text](https://github.com/brokebit/SolarDataLogger/blob/main/assets/GrafanaScreenshot.png?raw=true)

# To Do
- Fix math for INA226 current sensor and address twos compliment
- CMX60D10 Relay Driver
- Some way to configure it without recompiling 
- Redo design in KiCad instead of EasyEDA and add to repo
- Finish add onboard with INA226 and CMX60D10
- Code clean up. Comments, proper error handling, review memory managment, null point checks, etc
- Wifi Provisioning via BLE
- Add remote commands for reading voltage and current data

# Done
- WS2812 Driver
- OTA Updates

# Notes to self
## OTA
- Rememeber you need to configure partitions.csv

## Provisioning
- To make a QR Code for provisioning use the following JSON string and fill in appropriate values:
```{"ver":"v1","name":"PROV_123456","pop":"abcd12345","transport":"ble"}```
- Use the mobile app called "ESP BLE Provisioning"
- In menuconfig make sure that: Component config -> Bluetooth -> Bluetooth controller -> Controller Options is set to BLE not Dual Mode.

<p float="left">
  <img src="https://github.com/brokebit/SolarDataLogger/blob/main/assets/Brokebit-Cat-Logo.png?raw=true" width="200" />
  <img src="https://github.com/brokebit/SolarDataLogger/blob/main/assets/oshw-logo-800-px.png?raw=true" width="200" />
</p>

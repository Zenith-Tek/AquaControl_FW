# Aqua Control

This repository contains entire code for the Aqua Control project

## Features

- We have two nodes 1. Sender 2. Receiver
- Sender Reads the data from ultrasonic sensor attached to it and sends that data using lora communication
- Receiver reads that data from sender, controls the motor accordingly and also sends that data to supabase server and from there to application

## Prerequisites
- ESP-IDF installed ([ESP-IDF Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/))

## Installation
```bash
# clone the repository
https://github.com/JnanaPhani/AquaControl_FW.git

cd AquaControl_FW

# build the project
. $HOME/esp/esp-idf/export.sh
idf.py set-target esp32
idf.py build

# flash the firmware to ESP32
sudo chmod a+rw <port>
idf.py -p <port> build flash monitor

# monitor logs
idf.py monitor
```

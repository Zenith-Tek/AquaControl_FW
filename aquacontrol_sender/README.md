# AquaControl - Sender Node Firmware (ESP32-C3)

The **AquaControl Sender** is a battery-powered ESP32-C3 node designed to perform highly accurate ultrasonic water-level readings and securely transmit the telemetry to the Receiver node over LoRa. It is designed to run for 12+ months on a single Li-Po battery charged by a small 6V solar panel.

---

## 🛠️ Key Features
1. **Safe-Adaptive Sleep State Machine**:
   * Dynamically switches deep sleep cycles (from 15 seconds up to 15 minutes) based on whether the water pump is actively filling, idle, or near full.
2. **Unpaired & Shipping Power Optimization**:
   * Detects offline status via consecutive missed ACKs.
   * **Shipping Mode**: If offline and dark (solar voltage < 1.0V), sleeps for **2 hours** to conserve power during transit/storage.
   * **Offline Mode**: If offline and in light (solar voltage $\ge$ 1.0V), sleeps for **30 minutes**.
3. **Critical Battery Shutdown**:
   * If the battery level drops under **10%**, the node disables the ultrasonic sensor completely and wakes up only **twice a day** (12-hour sleep) to send a low-battery emergency alert.
4. **AES-256-GCM Cryptographic Security**:
   * Encrypts all sensor payloads using derived 256-bit AES keys with unique 12-byte IVs.
   * Authenticates frames with the packet header as Associated Data (AAD) to prevent replay/spoofing.
5. **On-Demand Power Gating**:
   * Employs P-channel MOSFETs to completely cut off the power rails of the LoRa and JSN-SR04 ultrasonic sensor prior to entering deep sleep, eliminating parasitic current leakage.

---

## 🔌 Pin Configuration (ESP32-C3)

| Component | Function | ESP32-C3 GPIO Pin |
|---|---|---|
| **LoRa (SX1278)** | MOSI | GPIO 6 |
| | MISO | GPIO 5 |
| | SCLK | GPIO 4 |
| | CS (NSS) | GPIO 7 |
| | RST | GPIO 8 |
| | DIO0 | GPIO 10 |
| | **Power Control (MOSFET)** | **GPIO 3** (Active LOW) |
| **JSN-SR04T** | Trigger | GPIO 0 |
| | Echo | GPIO 1 |
| | **Power Control (MOSFET)** | **GPIO 2** (Active LOW) |
| **Telemetry ADC** | Battery Voltage (Divider) | GPIO 9 (ADC1 Channel 8) |
| | Solar Panel Voltage | GPIO 18 (ADC1 Channel 4) |

---

## ⚡ Sleep Cycle Strategy

| State | Auto Control | Pump State | Water Level | Sleep Duration |
|---|---|---|---|---|
| **Active Filling** | Any | ON | $\le 70\%$ | **1 Minute** (60s) |
| **Active Filling** | Any | ON | $70\% - 80\%$ | **30 Seconds** |
| **Active Filling** | Any | ON | $> 80\%$ | **15 Seconds** |
| **Idle** | ON | OFF | $< 80\%$ | **5 Minutes** (300s) |
| **Idle** | OFF | OFF | $< 80\%$ | **15 Minutes** (900s) |
| **Safety Ceiling** | Any | OFF | $\ge 80\%$ | **2 Minutes** (120s) |
| **Shipping Mode** | Any | Any | Dark Box, Unpaired | **2 Hours** (7200s) |
| **Offline Mode** | Any | Any | Daylight, Unpaired | **30 Minutes** (1800s) |
| **Battery Critical** | Any | Any | $< 10\%$ | **12 Hours** (43200s) |

---

## 🚀 Building & Flashing

This project uses the ESP-IDF framework (V5.0 or later).

### 1. Set Up Environment
```bash
. /path/to/esp-idf/export.sh
```

### 2. Configure target
```bash
cd aquacontrol_sender/fw
idf.py set-target esp32c3
```

### 3. Build & Flash
```bash
idf.py build
idf.py -p [PORT] flash monitor
```

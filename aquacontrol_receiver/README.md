# AquaControl - Receiver Node Firmware (ESP32-S3)

The **AquaControl Receiver** is an ESP32-S3 mains-powered node. It serves as the gateway for the AquaControl system, receiving encrypted LoRa telemetry packets from the Sender node, executing automatic/manual water pump control, synchronizing time over SNTP, and uploading states to a Supabase database.

---

## 🛠️ Key Features
1. **AES-256-GCM Secure Handshake & ACK**:
   * Decrypts Incoming LoRa telemetry using derived 256-bit AES keys.
   * Generates secure, GCM-encrypted ACK packets to synchronize state (Motor, Automation) back to the Sender.
2. **1-Hour Manual Override Window**:
   * Activates a 1-hour automatic control bypass when the user manually toggles the motor via the physical switch or the mobile app (synced via Supabase).
3. **100% Full Safety Override**:
   * Forcibly shuts off the water pump immediately if the tank level reaches 100% capacity, regardless of manual override or system state, preventing floods.
4. **Solar Panel Health Diagnostics**:
   * Uses Wi-Fi and SNTP to synchronize and track real-world epoch time.
   * Logs active solar charging ($\ge 2.0\text{V}$) during daylight hours (8 AM to 5 PM) to NVS flash.
   * Triggers an alert if no charging is detected for **7 days** (dust/debris alert) or **20 days** (hardware failure alert).
5. **Supabase Database Integration**:
   * Uploads real-time telemetry, including battery capacity, solar voltage, and health alerts.

---

## 🔌 Pin Configuration (ESP32-S3)

| Component | Function | ESP32-S3 GPIO Pin |
|---|---|---|
| **LoRa (SX1278)** | MOSI | GPIO 11 |
| | MISO | GPIO 13 |
| | SCLK | GPIO 12 |
| | CS (NSS) | GPIO 10 |
| | RST | GPIO 9 |
| | DIO0 | GPIO 14 |
| **Relay Control** | Pump Relay Gate | GPIO 38 |
| **Manual Override**| Tactile Debounced Switch | GPIO 47 |

---

## 🚀 Building & Flashing

This project uses the ESP-IDF framework (V5.0 or later).

### 1. Set Up Environment
```bash
. /path/to/esp-idf/export.sh
```

### 2. Configure target
```bash
cd aquacontrol_receiver/fw
idf.py set-target esp32s3
```

### 3. Build & Flash
```bash
idf.py build
idf.py -p [PORT] flash monitor
```

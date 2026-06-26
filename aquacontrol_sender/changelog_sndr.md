# Technical Changelog - Aqua Control Sender

This document details the modifications and additions made to the **Sender** firmware since git commit `2c98efadafbf6471bd4f4bc5fb2ab9a56569e3d5`.

---

## [2026-06-26] - BLE OTA Integration & Advanced Power Optimizations

### 1. On-Demand BLE OTA Update System
* **LoRa Downlink Protocol**:
  * Updated `secure_ack_payload_t` in `lora_protocol.h` to include a `flags` byte (bit 0 indicates a BLE Enable trigger).
* **RTC Trigger State & Boot Redirection**:
  * Added `rtc_ble_enable_pending` to RTC-retained memory to track pending OTA states.
  * Added logic in `task_tx` to detect the BLE trigger bit in decrypted ACKs, set `rtc_ble_enable_pending = 1`, and trigger a software reset (`esp_restart()`).
  * Patched `app_main` to check the RTC flag immediately on boot; if active, it redirects to the BLE OTA task before initializing any other drivers (LoRa, ultrasonic) to save power.
* **NimBLE GATT Service & Flash Partitioning**:
  * Created `ble_ota.c` and `ble_ota.h` defining a custom BLE GATT service with Control (Start, Commit, Abort) and Data (raw chunk streaming) characteristics.
  * Writes the streamed firmware binary chunks to the passive OTA partition slot using the `esp_ota_ops` component.
  * Configured `sdkconfig.defaults` to enable the lightweight NimBLE Bluetooth stack, allocate dual 1MB OTA app slots, and support 4MB flash sizing.
* **Inactivity Watchdog & I2C Disabling**:
  * Added a 5-minute inactivity watchdog that de-initializes the NimBLE stack, resets `rtc_ble_enable_pending = 0`, and returns the Sender to deep sleep if the update process stalls.
  * Completely disabled I2C peripheral power and clocks at software/hardware levels and configured default I2C pins (`GPIO_NUM_2` and `GPIO_NUM_8`) as `GPIO_FLOATING` to prevent leakage.

### 2. Comprehensive Power Optimization Suite
* **Phase 1: Timing & CPU Scaling**:
  * Added `PRODUCTION_MODE` toggle to `main.h` to dynamically disable all diagnostic logs at startup.
  * Downclocked active CPU frequency from 160 MHz to 80 MHz, reducing active current by ~15 mA.
  * Reduced LoRa ACK receive wait timeout from 200 ms to 50 ms.
  * Reduced peripheral power stabilization delay to 20 ms.
* **Phase 2: Sensor Sampling Reductions**:
  * Reduced JSN-SR04T sample count from 5 to 3 and ping interval from 60 ms to 35 ms, halving active measurement time.
* **Phase 3: Smart Wakeup & Event-Based Telemetry**:
  * Implemented distance hysteresis (requires 2 consecutive readings differing by >= 4 cm from the last transmitted value) and battery shift (>= 2% delta) filters.
  * Added a 2-hour heartbeat backup and active-filling motor sync. If no telemetry conditions are met, the device skips LoRa TX and sleeps early.
* **Phase 4: Dynamic LoRa Power Control**:
  * Adjusts LoRa TX power (+10 dBm, +14 dBm, or +20 dBm) dynamically based on the RSSI of the last received ACK.

---

## 1. Low-Battery Protection & Alarm System
* **Emergency Process Shutdown**: Added logic in `main.c` to detect if the battery level drops below `10%`. If detected, the primary telemetry and sensor loops are disabled to protect the Lithium cell from over-discharge damage.
* **Low-Power Alarm Beacon**: Programmed the Sender to enter deep sleep and wake up twice a day (every 12 hours) to transmit an emergency low-battery packet (containing the `<10%` battery flag) to the Receiver, alerting the user before shutting down.

## 2. Initial Setup Power Optimization
* **Pairing Detection**: Integrated checks to detect if the Sender has successfully paired or completed a transmission.
* **Storage Guard**: If the device has not been paired, it enters an ultra-low-power deep sleep mode. This prevents the unit from draining its battery while resting on a shelf or during transport before installation and pairing.

## 3. Reset Reason Diagnostics
* **Hardware Boot Analysis**: Integrated boot-time hardware reset registry checks (`esp_reset_reason()`).
* **Bit-Packed Flags**: Map the reset reason (such as panics, software resets, watchdogs, or brownouts) into bits 1–3 of the encrypted LoRa telemetry payload `flags` byte. This enables the Receiver to act as a gateway and report the Sender's health to Supabase.

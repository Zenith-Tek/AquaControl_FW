# Technical Changelog - Aqua Control Sender

This document details the modifications and additions made to the **Sender** firmware since git commit `2c98efadafbf6471bd4f4bc5fb2ab9a56569e3d5`.

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

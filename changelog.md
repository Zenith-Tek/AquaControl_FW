# Changelog - AquaControl Firmware Updates

This changelog documents the new features, architecture refinements, security measures, and optimizations implemented in the AquaControl firmware since the last git commit.

---

## 1. Cryptographic Security & Communication Protocol
* **AES-256-GCM Secure Handshake**:
  * Integrated AES-256-GCM encryption/decryption on all LoRa transmissions.
  * Derived secure 256-bit AES keys from the user passcode using SHA-256.
  * Packets include a plaintext header (MAC addresses and sequence numbers) acting as authenticated data (AAD) to prevent replay attacks and spoofing.
* **Bidirectional Secure ACK**:
  * Defined `secure_ack_payload_t` and `secure_ack_packet_t` in `lora_protocol.h` to allow receiver-to-sender feedback.
  * The Receiver replies with an encrypted 2-byte ACK containing current `motor_state` and `auto_control_enabled` immediately upon receiving sensor telemetry.
  * The Sender waits in Rx mode for up to 200 ms post-transmission to capture and decrypt this ACK.

---

## 2. Power Optimization & Safe-Adaptive Sleep (Sender)
* **Safe-Adaptive Sleep State Machine**:
  * Implemented an RTC-backed state machine tracking the pump state (`rtc_motor_state`) and automation configuration (`rtc_auto_control`).
  * Dynamic wakeup interval calculation based on tank water level percentage:
    * **Active Filling Zone**: Sleeps for **15s** (level $> 80\%$), **30s** (level $70\% - 80\%$), or **1 min** (level $\le 70\%$) to ensure high-precision cutoffs.
    * **Idle Zone**: Sleeps for **5 min** (Auto ON) or **15 min** (Auto OFF).
    * **Safety Ceiling**: Enforces a **2 min** max sleep interval whenever water level $\ge 80\%$ to capture manual-start overflows.
* **Unpaired/Shipping Power Savings**:
  * Tracks consecutive missed ACKs using `rtc_missed_acks` in RTC memory.
  * If the Sender fails to receive an ACK for 3 consecutive attempts (meaning it is unpaired or the Receiver is offline):
    * **Shipping Mode (Solar Voltage < 1.0V)**: Suggests the unit is sealed inside a dark shipping box or storage. Sleeps for **2 hours** between checks, ensuring the battery remains charged for 5+ years.
    * **Daylight Offline Mode (Solar Voltage >= 1.0V)**: Suggests the unit is mounted in daylight but not yet paired. Sleeps for **30 minutes** to minimize setup drain.
  * Automatically reverts to fast adaptive intervals immediately when a single valid ACK is received.
* **Peripheral Power Gating**:
  * Ultrasonic sensors and the LoRa transceiver are power-gated and powered off completely before entering deep sleep.
* **Critical Low-Battery Emergency Shutdown**:
  * If the battery level drops below `10%`, the Sender enters a deep low-power shutdown state.
  * Shuts down the ultrasonic distance sensor completely to eliminate measurement current consumption.
  * Powers on the LoRa transceiver only to transmit an emergency low-battery packet.
  * Enforces a **12-hour deep sleep** cycle (emergency status broadcasted twice a day).
  * The Receiver handles this packet by instantly shutting off the relay (pump) for safety, updating the Supabase alert message to `"Sender battery critically low (<10%)! Main process shutdown."`, and reporting it to the user.
  * The Sender automatically resumes normal adaptive operations once solar charging restores the battery to $\ge 10\%$.

---

## 3. Solar Health Monitoring & Diagnostics (Receiver)
* **Time Synchronization**:
  * Configured the ESP-IDF SNTP client on the Receiver to obtain synchronized network time upon Wi-Fi connection.
* **Solar Health Tracking**:
  * Monitors solar charging activity ($\ge 2.0\text{V}$) during daylight hours (8 AM to 5 PM).
  * Automatically stores the timestamp of the last active charge in NVS flash to persist across reboots.
  * Calculates elapsed time since the last successful charge to generate user-friendly alert messages:
    * **7 days without charge**: `"Clean solar panel (dust/debris detected)"`
    * **20 days without charge**: `"Contact customer care (solar hardware failure)"`
    * **Standard**: `"Normal"`

---

## 4. Manual Override & Flood Safety (Receiver)
* **1-Hour Manual Override Window**:
  * Implemented a 1-hour manual bypass timer when the user manually toggles the motor via the physical switch or the mobile app (polled via Supabase).
  * Bypasses the auto ON/OFF thresholds for 1 hour to allow manual watering or tank operations.
* **100% Safety Override Cutoff**:
  * If the water level reaches **100% capacity**, the system instantly overrides the manual bypass and shuts off the relay to prevent accidental flooding.

---

## 5. Web/Database Integration
* **Supabase JSON Payload Extension**:
  * Expanded the database telemetry format to upload `battery_percent`, `solar_voltage`, and the dynamic `alert_message` alongside water levels.
* **Wi-Fi & Provisioning Refinements**:
  * Refactored connection handling in `wifi.c` to gracefully retry and initialize network services.

---

## 6. System Diagnostics & Over-The-Air (OTA) Updates (Receiver)
* **Boot-Time Crash Diagnostics**:
  * Added checking for ELF core dumps in the flash `coredump` partition on boot (Receiver).
  * Dynamically extracts crashed task names, fault program counters, and backtraces, and uploads them to Supabase via the `report_crash` RPC.
  * Erases core dumps from flash upon successful upload to prevent double reporting.
  * **Sender Crash Gateway**: Enabled Sender to read `esp_reset_reason()` at boot and encode crash reason codes (bits 1-3) into the telemetry payload `flags`. The Receiver decodes this payload and asynchronously uploads the Sender's reboot reason to Supabase via the `report_crash` RPC.
* **Secure OTA Firmware Updates**:
  * Implemented an HTTPS-based background OTA update task with exponential backoff retry.
  * Added security check verifying that update URLs originate from the trusted Supabase Storage prefix associated with the project's compiled-in `SUPABASE_URL`.
  * Evaluates and parses the `system_control` configuration row on Supabase every 5 minutes to trigger canary/fleet updates.
* **Automatic Rollback & Self-Test**:
  * Configured ESP-IDF app rollback support (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`).
  * Runs a hard-gated verification check (peripherals initialized and Wi-Fi IP acquired).
  * Automatically rolls back to the previous firmware slot in case of a crash or connection failure, ensuring high availability.
* **Database Schema & RPC Functions**:
  * Added [functions.sql](file:///home/zenithtek/AquaControl_FW/DB/functions.sql) containing stored procedures `report_crash` and `report_ota_status` using `SECURITY DEFINER` to securely log system operations and bypass Row-Level Security.
  * Aligned column properties across tables (`system_control`, `crash_logs`, `ota_status`) to strictly match types sent/polled by the firmware.
  * Documented Row-Level Security (RLS) anonymous access rules for telemetry post and update checks.
* **Offline Resilience & Memory Safety (Receiver)**:
  * **Non-Blocking DHCP Timeout**: Refactored `connect_wifi` to use a 10-second DHCP timeout. If Wi-Fi/DHCP is offline, the boot sequence falls back to "offline mode," allowing local LoRa operations and relay control to function immediately rather than blocking indefinitely.
  * **Offline Boot Optimization**: Bypasses initial crash reporting and configuration sync during offline boots to eliminate a 20-second startup delay. All actions automatically resume as soon as the network connection is restored.
  * **Memory Safety & Task Cleanups**: Resolved a potential memory leak of the JSON print buffer if `xTaskCreate` fails during Sender crash logging. Resolved a similar memory leak of the `ota_job_t` struct if `xTaskCreate` fails during OTA task spawning.
  * **NVS Wear Reduction**: Added a 60-second rate limiter to NVS bindings commits in `zt_lora.c`. Prevents writing sequence numbers to flash too frequently during fast-transmission test or error modes, dramatically extending flash lifetime.

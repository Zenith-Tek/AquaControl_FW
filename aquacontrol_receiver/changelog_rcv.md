# Technical Changelog - Aqua Control Receiver

This document details the modifications and additions made to the **Receiver** firmware since git commit `2c98efadafbf6471bd4f4bc5fb2ab9a56569e3d5`.

---

## [2026-06-26] - BLE OTA Downlink Relay Integration

### 1. Database Schema & Control Integrations
* **Supabase Control State Expansion**:
  * Updated `read_control_state_from_supabase()` and `parse_realtime_payload()` in `supabase.c` to query and parse the new `sender_ble_enable` column from the `device_control` table in Supabase.
  * Supports both regular REST HTTP polling and instant Phoenix WebSocket real-time changes.
* **Asynchronous Database Feedback**:
  * Created `update_sender_ble_enable_in_supabase()` to execute a PATCH request setting `sender_ble_enable` back to `false` in Supabase. This automatically resets the dashboard UI once the update sequence is triggered.

### 2. LoRa Protocol & Transmission Flow
* **Downlink Flag Embedding**:
  * Updated `secure_ack_payload_t` in `lora_protocol.h` to include a `flags` byte.
  * Configured the ACK packer in `zt_lora.c` to parse `g_sender_ble_enable_req` and set flag bit 0 when preparing the encrypted ACK payload.
  * Triggers database update to reset the flag immediately after transmitting the ACK packet over LoRa.

---

## 1. System Resilience & Offline Reliability
* **Non-Blocking DHCP Timeout**: Refactored the Wi-Fi connection logic in `wifi.c` to prevent blocking the boot sequence if DHCP is delayed or the router is offline. Added a 10-second timeout. If it times out, the device logs a warning and proceeds in **offline mode**, booting the LoRa radio and local relay control tasks immediately.
* **Offline Boot Optimization**: Bypasses initial crash report uploads and Supabase configuration syncs during offline boots in `main.c` to eliminate a 20-second startup delay. All actions automatically resume in the background once Wi-Fi is restored.
* **Local Control Lag Prevention**: Modified `update_control_state_from_esp32` in `supabase.c` to return immediately when offline. This prevents the local manual override button (GPIO task) from freezing for 10 seconds during socket connection timeouts.

## 2. Boot-Time Crash Reporting
* **ELF Core Dump Parser**: Added a boot check for post-crash ELF core dumps stored in the flash `coredump` partition.
* **RPC Diagnostic Logger**: Dynamically extracts the crashed task name, fault program counter (PC), and backtrace frames. Formats the data as a cJSON payload and uploads it via the `report_crash` Supabase RPC function.
* **Cleanups**: Erases the flash core dump partition after a successful upload to prevent double reporting.

## 3. Sender Crash Telemetry Gateway
* **Reset Reason Extraction**: Added logic to decode the Sender's reset reason (bits 1–3) from the decrypted LoRa telemetry payload `flags` byte.
* **Asynchronous Upload**: Translates the 3-bit flag into human-readable strings (e.g. `Panic`, `Brownout`, `Watchdog`) and spawns a background thread to call the `report_crash` Supabase RPC with the Sender's MAC address.
* **Memory Safety**: Added checks to verify task creation status (`xTaskCreate` return value) and free the JSON print buffer immediately in the event of task allocation failure.

## 4. Secure Over-The-Air (OTA) Updates & Rollback Self-Test
* **Background HTTPS OTA**: Implemented a background OTA task (`ota.c`) with exponential backoff retry.
* **URL Prefix Trust Check**: Added a security verification step ensuring the binary download URL belongs strictly to the project's compiled-in public Supabase Storage prefix (`SUPABASE_URL`), protecting the system against malicious firmware injections.
* **Bootloader Rollback Integration**: Configured Kconfig-based bootloader rollback (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`).
* **Health Validation Gate**: Implemented a tiered self-test (`ota_selftest.c`) requiring successful peripheral initialization and Wi-Fi IP acquisition. If the health checks fail or the device crashes on the new firmware slot, the bootloader automatically reverts to the previous working slot.

## 5. Hardware & Flash Longevity
* **NVS Wear Protection**: Implemented a 60-second rate limiter on committing sequence number updates to NVS in `zt_lora.c`. This prevents excessive flash write wear from high-frequency packets and extends the physical lifespan of the flash chip.
* **Flash Partition Resize**: Configured `partitions.csv` to replace the single-app partition with a custom 2-slot OTA table layout (1.5 MB `ota_0` and `ota_1` slots) plus a 64 KB `coredump` partition.
* **4MB Flash Config**: Configured `sdkconfig` to build with a 4MB flash size (`CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`) to accommodate the expanded code and dual OTA partitions.

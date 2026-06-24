# Phase 1 Walkthrough - Fixes & Foundation

We have successfully executed and verified all changes in Phase 1. Both the Sender and Receiver firmwares compile cleanly with zero warnings.

## Changes Made

### 1. Sender Header Variable Bug Fix
* **Issue**: `sleep_enter_time` was declared `static RTC_DATA_ATTR` in the header `utils.h`, causing duplicate allocations in different compilation units.
* **Fix**:
  * Changed the header declaration to `extern struct timeval sleep_enter_time;` in [utils.h](file:///home/zenithtek/AquaControl_FW/aquacontrol_sender/fw/main/inc/utils.h#L24-L28).
  * Defined it globally as `RTC_DATA_ATTR struct timeval sleep_enter_time;` in [utils.c](file:///home/zenithtek/AquaControl_FW/aquacontrol_sender/fw/main/src/utils.c#L9-L13).

### 2. Receiver Partition Expansion
* **Issue**: The Receiver binary size was at 99% of its 1MB partition limit, generating build size warnings and restricting future development.
* **Fix**:
  * Created a custom partition layout file [partitions.csv](file:///home/zenithtek/AquaControl_FW/aquacontrol_receiver/fw/partitions.csv) allocating **1.875MB** (1920KB) for the factory application.
  * Updated [sdkconfig](file:///home/zenithtek/AquaControl_FW/aquacontrol_receiver/fw/sdkconfig#L395-L408) to compile with the custom partition file.
  * This increased the free app space from **1%** to **47%** (leaving ~950KB for future logic).

### 3. Receiver Communication Fail-Safe
* **Issue**: If the Sender drops offline while the pump is active, the pump will run indefinitely, causing water overflow or damage.
* **Fix**:
  * Added `last_lora_recv_time_ms` tracking and a `LORA_FAILSAFE_TIMEOUT_MS` threshold (5 minutes) in [utils.h](file:///home/zenithtek/AquaControl_FW/aquacontrol_receiver/fw/main/inc/utils.h#L11-L15).
  * Updated the timestamp in [zt_lora.c](file:///home/zenithtek/AquaControl_FW/aquacontrol_receiver/fw/main/src/zt_lora.c#L93-L101) only when a packet is successfully parsed.
  * Added a timeout check in the periodic `device_control_task` loop inside [supabase.c](file:///home/zenithtek/AquaControl_FW/aquacontrol_receiver/fw/main/src/supabase.c#L260-L285). If the motor is active and the timeout is exceeded, the motor is turned off immediately, and the status update is pushed to Supabase.
  * Fixed compilation warnings by removing unused variables in `wifi.c` and commenting out unused code in `utils.c`.

---

## Verification Results

### Sender Firmware Build
```bash
lora_send.bin binary size 0x38eb0 bytes. Smallest app partition is 0x100000 bytes. 0xc7150 bytes (78%) free.
```
* **Status**: Complete & Verified (0 warnings, 0 errors).

### Receiver Firmware Build
```bash
lora.bin binary size 0xfc440 bytes. Smallest app partition is 0x1e0000 bytes. 0xe3bc0 bytes (47%) free.
```
* **Status**: Complete & Verified (0 warnings, 0 errors).

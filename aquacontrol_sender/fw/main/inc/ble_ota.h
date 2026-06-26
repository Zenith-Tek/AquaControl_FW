#ifndef __BLE_OTA_H__
#define __BLE_OTA_H__

/**
 * @brief Initialize BLE (NimBLE stack) and enter OTA Update mode.
 * 
 * This function is blocking. It advertises BLE and waits for connections.
 * If OTA succeeds, it restarts. If it times out or fails, it exits BLE mode,
 * clears the BLE pending RTC flag, and enters deep sleep.
 */
void start_ble_ota_mode(void);

#endif // __BLE_OTA_H__

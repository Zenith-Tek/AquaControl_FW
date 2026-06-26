#include "main.h"
#include "ble_ota.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "lora_protocol.h"
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"
#include <math.h>

#define SENDER_PASSCODE "384910"

extern float ultrasonic_data;
RTC_DATA_ATTR uint32_t sender_seq_num = 0;
RTC_DATA_ATTR uint8_t rtc_motor_state = 0;
RTC_DATA_ATTR uint8_t rtc_auto_control = 1;
RTC_DATA_ATTR uint32_t rtc_missed_acks = 0;
RTC_DATA_ATTR float rtc_last_sent_distance = -1.0f;
RTC_DATA_ATTR uint8_t rtc_last_sent_battery = 0;
RTC_DATA_ATTR uint32_t rtc_last_tx_timestamp = 0;
RTC_DATA_ATTR uint8_t rtc_consecutive_changes = 0;
RTC_DATA_ATTR int rtc_last_ack_rssi = -95;
RTC_DATA_ATTR uint8_t rtc_ble_enable_pending = 0;
struct timeval tv;
static uint8_t g_sender_reboot_reason_flag = 0;

#define TAG_INFO "APP"

void print_system_memory_status() {
  ESP_LOGI(TAG_INFO, "========== Chip Information "
                     "===========================================");
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  const char *chip_model;
  switch (chip_info.model) {
  case CHIP_ESP32:
    chip_model = "ESP32";
    break;
  case CHIP_ESP32S2:
    chip_model = "ESP32-S2";
    break;
  case CHIP_ESP32S3:
    chip_model = "ESP32-S3";
    break;
  case CHIP_ESP32C3:
    chip_model = "ESP32-C3";
    break;
  case CHIP_ESP32H2:
    chip_model = "ESP32-H2";
    break;
  default:
    chip_model = "Unknown";
    break;
  }
  ESP_LOGI(TAG_INFO, "Chip model: %s", chip_model);

  ESP_LOGI(TAG_INFO, "CPU cores: %d", chip_info.cores);
  ESP_LOGI(TAG_INFO, "Silicon revision: %d", chip_info.revision);

  uint32_t flash_size = 0;
  esp_flash_get_size(NULL, &flash_size); // <-- updated function
  ESP_LOGI(TAG_INFO, "Flash size: %lu MB",
           (unsigned long)flash_size / (1024 * 1024));

  const esp_app_desc_t *app_desc = esp_app_get_description();
  gettimeofday(&tv, NULL);
  ESP_LOGI(TAG_INFO, "========== Program Version "
                     "============================================");
  ESP_LOGI(TAG_INFO, "[APP] Name: %s", app_desc->project_name);

  ESP_LOGI(TAG_INFO, "[APP] Version: %s", APP_VERSION);
  ESP_LOGI(TAG_INFO, "[APP] Compile Date: %s", app_desc->date);
  ESP_LOGI(TAG_INFO, "[APP] Compile Time: %s", app_desc->time);
  ESP_LOGI(TAG_INFO, "========== Heap Information "
                     "===========================================");
  ESP_LOGI(TAG_INFO, "Total free heap: %lu bytes",
           (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
  ESP_LOGI(TAG_INFO, "Minimum free heap since boot: %lu bytes",
           (unsigned long)heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
  ESP_LOGI(TAG_INFO, "Internal RAM free: %lu bytes",
           (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

  ESP_LOGI(TAG_INFO, "========== Stack Information "
                     "==========================================");
  ESP_LOGI(TAG_INFO, "Current task stack high water mark: %lu bytes",
           (unsigned long)uxTaskGetStackHighWaterMark(NULL));

  ESP_LOGI(TAG_INFO, "========== Flash Partition Information "
                     "================================");
  const esp_partition_t *part = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
  if (part != NULL) {
    ESP_LOGI(TAG_INFO, "App partition size: %lu bytes",
             (unsigned long)part->size);
  } else {
    ESP_LOGI(TAG_INFO, "App partition not found!");
  }

  ESP_LOGI(TAG_INFO, "========================================================="
                     "==============");
}

int lora_read_reg(int reg);
void lora_receive(void);

static bool is_channel_busy(void) {
  // Put transceiver in receive mode to listen to carrier
  lora_receive();
  vTaskDelay(pdMS_TO_TICKS(10)); // Allow LNA and RSSI to stabilize

  // Read RSSI register 0x1b
  int rssi_reg = lora_read_reg(0x1b);
  int rssi = rssi_reg - 164; // 433MHz offset is 164

  // If RSSI is above -95 dBm, channel is busy
  if (rssi > -95) {
    ESP_LOGW("LORA_CSMA", "Channel busy! RSSI: %d dBm", rssi);
    return true;
  }
  return false;
}

void task_tx(void *pvParameters) {
  ESP_LOGI(pcTaskGetName(NULL), "Start");

  // 1. Check battery percentage first to see if we should enter emergency low
  // power shutdown
  uint8_t batt = read_battery_percentage();
  uint8_t solar_volt_x10 = read_solar_voltage();

  if (batt < 10) {
    ESP_LOGE(pcTaskGetName(NULL),
             "Battery is critically low (%d%% < 10%%)! Entering shutdown mode.",
             batt);

    // Power ON LoRa module only (keep ultrasonic OFF to save power)
    power_lora_only_on();
    vTaskDelay(pdMS_TO_TICKS(20));

    // Derive AES key
    uint8_t aes_key[32];
    mbedtls_sha256((const unsigned char *)SENDER_PASSCODE,
                   strlen(SENDER_PASSCODE), aes_key, 0);

    secure_lora_packet_t packet;
    esp_efuse_mac_get_default(packet.src_mac);
    memset(packet.dest_mac, 0xFF, 6); // Broadcast
    packet.seq_num = sender_seq_num++;

    secure_payload_t payload;
    payload.distance_cm = -1.0f; // Invalid/emergency indicator
    payload.battery_percent = batt;
    payload.solar_voltage_x10 = solar_volt_x10;
    payload.flags =
        1 | (g_sender_reboot_reason_flag
             << 1); // flag bit 0 = low battery, bits 1-3 = reboot reason

    esp_fill_random(packet.iv, 12);

    mbedtls_gcm_context gcm_ctx;
    mbedtls_gcm_init(&gcm_ctx);
    mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, aes_key, 256);
    mbedtls_gcm_crypt_and_tag(
        &gcm_ctx, MBEDTLS_GCM_ENCRYPT, sizeof(secure_payload_t), packet.iv, 12,
        (const unsigned char *)&packet, 16, (const unsigned char *)&payload,
        packet.ciphertext, 16, packet.tag);
    mbedtls_gcm_free(&gcm_ctx);

    // Transmit emergency packet
    lora_send_packet((uint8_t *)&packet, sizeof(packet));
    ESP_LOGI(pcTaskGetName(NULL),
             "Sent EMERGENCY packet -> Seq: %lu, Battery: %d%%, Solar: %.1fV",
             (unsigned long)packet.seq_num, payload.battery_percent,
             (float)payload.solar_voltage_x10 / 10.0f);

    // Immediately power OFF LoRa to conserve power
    power_peripherals_off();

    // Sleep for 12 hours (43,200 seconds)
    uint32_t emergency_sleep_sec = 43200;
    // uint32_t emergency_sleep_sec = 4;
    ESP_LOGI(pcTaskGetName(NULL),
             "Entering emergency deep sleep for 12 hours...");
    deepsleep(emergency_sleep_sec);

    while (1) {
      vTaskDelay(1);
    }
  }

  // --- Normal Process (Battery >= 10%) ---
  // 2. Power ON peripherals (LoRa and JSN-SR04)
  power_peripherals_on();
  // Allow 20ms for LDO and sensor power rails to stabilize
  vTaskDelay(pdMS_TO_TICKS(20));

  // 3. Perform the distance measurement (median filter)
  float dist = read_filtered_ultrasonic_distance();

  // --- Hysteresis & Event-based Check ---
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  uint32_t current_time = tv_now.tv_sec;

  bool force_send = (rtc_last_tx_timestamp == 0) ||
                    ((current_time - rtc_last_tx_timestamp) >= 7200);
  bool battery_changed = (rtc_last_sent_battery == 0) ||
                         (abs((int)batt - (int)rtc_last_sent_battery) >= 2);

  bool distance_changed = false;
  if (rtc_last_sent_distance < 0.0f) {
    distance_changed = true;
  } else if (dist > 0.0f) {
    if (fabs(dist - rtc_last_sent_distance) >= 4.0f) {
      rtc_consecutive_changes++;
      if (rtc_consecutive_changes >= 2) {
        distance_changed = true;
      }
    } else {
      rtc_consecutive_changes = 0;
    }
  }

  bool is_filling = (rtc_motor_state == 1);
  bool change_detected = distance_changed || battery_changed;
  bool do_transmit = is_filling || change_detected || force_send;

  if (!do_transmit) {
    ESP_LOGI(pcTaskGetName(NULL),
             "Suppressing LoRa TX. Dist: %.2f (last: %.2f), Batt: %d%% (last: "
             "%d%%), Elapsed: %lu s",
             dist, rtc_last_sent_distance, batt, rtc_last_sent_battery,
             (unsigned long)(current_time - rtc_last_tx_timestamp));
    power_peripherals_off();
    goto calculate_sleep;
  }

  // Set TX Power dynamically based on last recorded RSSI (Phase 4)
  int tx_power = 20;
  if (rtc_last_ack_rssi > -70) {
    tx_power = 10;
  } else if (rtc_last_ack_rssi > -85) {
    tx_power = 14;
  } else {
    tx_power = 20;
  }
  lora_set_tx_power(tx_power);
  ESP_LOGI(pcTaskGetName(NULL),
           "Using dynamic TX Power: %d dBm (Last ACK RSSI: %d dBm)", tx_power,
           rtc_last_ack_rssi);

  // 4. CSMA/CA Carrier Sense
  int retries = 5;
  while (retries-- > 0) {
    if (!is_channel_busy()) {
      break;
    }
    uint32_t backoff = 50 + (esp_random() % 200);
    ESP_LOGI("LORA_CSMA", "Channel busy, backing off for %lu ms...",
             (unsigned long)backoff);
    vTaskDelay(pdMS_TO_TICKS(backoff));
  }

  // 5. Build, Encrypt, and Transmit Packet
  uint8_t aes_key[32];
  mbedtls_sha256((const unsigned char *)SENDER_PASSCODE,
                 strlen(SENDER_PASSCODE), aes_key, 0);

  secure_lora_packet_t packet;
  esp_efuse_mac_get_default(packet.src_mac);
  memset(packet.dest_mac, 0xFF, 6); // Broadcast destination
  packet.seq_num = sender_seq_num++;

  secure_payload_t payload;
  payload.distance_cm = dist;
  payload.battery_percent = batt;
  payload.solar_voltage_x10 = solar_volt_x10;
  payload.flags =
      (g_sender_reboot_reason_flag << 1); // bits 1-3 = reboot reason

  esp_fill_random(packet.iv, 12);

  mbedtls_gcm_context gcm_ctx;
  mbedtls_gcm_init(&gcm_ctx);
  mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, aes_key, 256);
  mbedtls_gcm_crypt_and_tag(
      &gcm_ctx, MBEDTLS_GCM_ENCRYPT, sizeof(secure_payload_t), packet.iv, 12,
      (const unsigned char *)&packet, 16, (const unsigned char *)&payload,
      packet.ciphertext, 16, packet.tag);
  mbedtls_gcm_free(&gcm_ctx);

  // Send the secure packet over LoRa
  lora_send_packet((uint8_t *)&packet, sizeof(packet));
  ESP_LOGI(pcTaskGetName(NULL),
           "Sent secure packet -> Seq: %lu, Distance: %.2f cm, Battery: %d%%, "
           "Solar: %.1fV, MAC: %02X:%02X:%02X:%02X:%02X:%02X",
           (unsigned long)packet.seq_num, payload.distance_cm,
           payload.battery_percent, (float)payload.solar_voltage_x10 / 10.0f,
           packet.src_mac[0], packet.src_mac[1], packet.src_mac[2],
           packet.src_mac[3], packet.src_mac[4], packet.src_mac[5]);

  // Check for packet loss
  int lost = lora_packet_lost();
  if (lost != 0) {
    ESP_LOGW(pcTaskGetName(NULL), "%d packets lost", lost);
  }

  // --- Wait for ACK ---
  ESP_LOGI(pcTaskGetName(NULL), "Listening for ACK...");
  lora_receive();
  bool ack_received = false;
  secure_ack_packet_t ack_packet;
  int rx_timeout = 50; // ms
  while (rx_timeout > 0) {
    if (lora_received()) {
      int rx_len =
          lora_receive_packet((uint8_t *)&ack_packet, sizeof(ack_packet));
      if (rx_len == sizeof(secure_ack_packet_t)) {
        ack_received = true;
        break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
    rx_timeout -= 5;
  }

  if (ack_received) {
    secure_ack_payload_t ack_payload;
    mbedtls_gcm_context gcm_ctx_ack;
    mbedtls_gcm_init(&gcm_ctx_ack);
    mbedtls_gcm_setkey(&gcm_ctx_ack, MBEDTLS_CIPHER_ID_AES, aes_key, 256);
    int ret = mbedtls_gcm_crypt_and_tag(
        &gcm_ctx_ack, MBEDTLS_GCM_DECRYPT, sizeof(secure_ack_payload_t),
        ack_packet.iv, 12, (const unsigned char *)&ack_packet, 16,
        ack_packet.ciphertext, (unsigned char *)&ack_payload, 16,
        ack_packet.tag);
    mbedtls_gcm_free(&gcm_ctx_ack);
    if (ret == 0) {
      rtc_motor_state = ack_payload.motor_state;
      rtc_auto_control = ack_payload.auto_control_enabled;
      rtc_missed_acks = 0; // Reset missed ACK counter

      // Update historical variables upon successful transmission and ACK
      // receipt
      rtc_last_sent_distance = dist;
      rtc_last_sent_battery = batt;
      rtc_last_tx_timestamp = current_time;
      rtc_consecutive_changes = 0;

      // Record last RSSI
      rtc_last_ack_rssi = lora_packet_rssi();

      ESP_LOGI(pcTaskGetName(NULL),
               "Decrypted ACK -> Motor: %d, AutoControl: %d, RSSI: %d dBm, "
               "Flags: %02X",
               rtc_motor_state, rtc_auto_control, rtc_last_ack_rssi,
               ack_payload.flags);

      if (ack_payload.flags & 0x01) {
        rtc_ble_enable_pending = 1;
        ESP_LOGW(pcTaskGetName(NULL),
                 "BLE OTA mode requested! Restarting into BLE mode...");
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
      }
    } else {
      rtc_missed_acks++;       // Count GCM failure as missed ACK
      rtc_last_ack_rssi = -95; // Reset to max power on error
      ESP_LOGE(pcTaskGetName(NULL), "ACK decryption failed (code %d)!", ret);
    }
  } else {
    rtc_missed_acks++;       // Increment missed ACK counter
    rtc_last_ack_rssi = -95; // Reset to max power on error
    ESP_LOGW(pcTaskGetName(NULL),
             "No ACK received within 50ms (Consecutive missed: %lu)",
             (unsigned long)rtc_missed_acks);
  }

  // 6. Power OFF peripherals to conserve energy
  power_peripherals_off();

calculate_sleep:
  // 7. Calculate Dynamic Sleep Interval
  uint32_t sleep_time_sec = 300; // Default 5 minutes
  float water_level_pct = 0.0f;

  if (rtc_missed_acks >= 3) {
    float solar_volt = (float)solar_volt_x10 / 10.0f;
    if (solar_volt < 1.0f) {
      // Boxed/Shipping state: Dark box and offline. Sleep 2 hours to conserve
      // battery.
      sleep_time_sec = 7200;
      ESP_LOGW(pcTaskGetName(NULL),
               "Unpaired/Offline in DARK (Solar: %.2fV). Shipping mode: "
               "sleeping for 2 hours.",
               solar_volt);
    } else {
      // Offline state: Daylight but offline. Sleep 30 minutes to save power.
      sleep_time_sec = 1800;
      ESP_LOGW(pcTaskGetName(NULL),
               "Unpaired/Offline in LIGHT (Solar: %.2fV). Offline mode: "
               "sleeping for 30 minutes.",
               solar_volt);
    }
  } else {
    float tank_height = 200.0f;
    float min_dist = 20.0f;
    if (dist > 0.0f && dist < tank_height) {
      water_level_pct =
          ((tank_height - dist) / (tank_height - min_dist)) * 100.0f;
    }
    if (water_level_pct < 0.0f)
      water_level_pct = 0.0f;
    if (water_level_pct > 100.0f)
      water_level_pct = 100.0f;

    if (rtc_motor_state == 1) {
      // Active filling
      if (water_level_pct <= 70.0f) {
        sleep_time_sec = 60; // 1 minute
      } else if (water_level_pct <= 80.0f) {
        sleep_time_sec = 30; // 30 seconds
      } else {
        sleep_time_sec = 15; // 15 seconds
      }
    } else {
      // Pump is OFF
      if (water_level_pct < 80.0f) {
        if (rtc_auto_control == 0) {
          sleep_time_sec = 900; // 15 minutes (Auto OFF)
        } else {
          sleep_time_sec = 300; // 5 minutes (Auto ON)
        }
      } else {
        sleep_time_sec = 120; // 2 minutes (Safety ceiling near full)
      }
    }
    ESP_LOGI(pcTaskGetName(NULL), "Normal state active. Water Level: %.1f%%",
             water_level_pct);
  }

  ESP_LOGI(pcTaskGetName(NULL), "Entering deep sleep for %lu seconds...",
           (unsigned long)sleep_time_sec);
  deepsleep(sleep_time_sec);

  // In case deep sleep fails
  while (1) {
    vTaskDelay(1);
  }
}

void app_main(void) {
#if PRODUCTION_MODE
  esp_log_level_set("*", ESP_LOG_NONE);
#else
  ESP_LOGE(pcTaskGetName(NULL), "DEBUG MODE ACTIVE");
#endif

  if (rtc_ble_enable_pending == 1) {
    start_ble_ota_mode();
  }

  // Determine reboot reason
  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_SW) {
    g_sender_reboot_reason_flag = 1;
  } else if (reason == ESP_RST_PANIC) {
    g_sender_reboot_reason_flag = 2;
  } else if (reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT ||
             reason == ESP_RST_WDT) {
    g_sender_reboot_reason_flag = 3;
  } else if (reason == ESP_RST_BROWNOUT) {
    g_sender_reboot_reason_flag = 4;
  } else if (reason == ESP_RST_EXT) {
    g_sender_reboot_reason_flag = 5;
  } else if (reason != ESP_RST_POWERON && reason != ESP_RST_DEEPSLEEP) {
    g_sender_reboot_reason_flag = 6;
  }

  print_system_memory_status();

  if (lora_init() == 0) {
    ESP_LOGE(pcTaskGetName(NULL), "Does not recognize the module");
    while (1) {
      vTaskDelay(1);
    }
  }

  ESP_LOGI(pcTaskGetName(NULL), "Frequency is 433MHz");
  lora_set_frequency(433e6); // 433MHz
  lora_enable_crc();

  int cr = 1;
  int bw = 7;
  int sf = 7;

  lora_set_coding_rate(cr);
  ESP_LOGI(pcTaskGetName(NULL), "coding_rate=%d", cr);

  lora_set_bandwidth(bw);
  ESP_LOGI(pcTaskGetName(NULL), "bandwidth=%d", bw);

  lora_set_spreading_factor(sf);
  ESP_LOGI(pcTaskGetName(NULL), "spreading_factor=%d", sf);

  ultrasonic_init();
  xTaskCreate(&task_tx, "TX", 1024 * 4, NULL, 5, NULL);
}

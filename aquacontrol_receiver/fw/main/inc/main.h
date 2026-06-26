#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "utils.h"
#include <esp_log.h>
#include "rc_gpio.h"

// Standard libraries
#include <sys/time.h>

// ESP-IDF libraries
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
// ESP-IDF libraries
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

// Required ESP-IDF headers
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_app_format.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"


// #include "wifi_provisioning/manager.h"
// #include "wifi_provisioning/scheme_softap.h"

// #define WIFI_CONNECTED_EVENT_TIMEOUT (3000/portTICK_PERIOD_MS)

void task_rx(void *pvParameters);
void print_system_memory_status();
void initialise_lora();

#ifndef APP_VERSION
#define APP_VERSION "2.0.0"
#endif
#define FIRMWARE_VERSION APP_VERSION
#define ENABLE_OTA 1

#endif
#ifndef __MAIN_H__
#define __MAIN_H__

#define PRODUCTION_MODE 0

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lora.h"
#include "utils.h"
// Standard libraries
#include <sys/time.h>

// ESP-IDF libraries
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "spi_flash_mmap.h"

void print_system_memory_status();
void task_tx(void *pvParameters);

#endif
#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "utils.h"
#include "lora.h"
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

void print_system_memory_status();
void task_tx(void *pvParameters); 

#endif
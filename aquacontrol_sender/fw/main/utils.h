#ifndef __UTILS_H__
#define __UTILS_H__

#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "sdkconfig.h"
#include "esp_sleep.h"
#include "esp_wake_stub.h"
#include "driver/rtc_io.h"
#include "rtcwake.h"
#include "driver/gpio.h"
#include "esp_timer.h"

extern float ultrasonic_data;
// // sleep_enter_time stored in RTC memory
static RTC_DATA_ATTR struct timeval sleep_enter_time;

void ultrasonic_init(void);
void deepsleep(void);

#endif

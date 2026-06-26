#ifndef __UTILS_H__
#define __UTILS_H__

#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
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

#define TRIG_GPIO GPIO_NUM_21
#define ECHO_GPIO GPIO_NUM_20
#define LORA_PWR_GPIO GPIO_NUM_9
#define JSN_PWR_GPIO GPIO_NUM_10
#define BAT_IN_GPIO GPIO_NUM_1
#define SOL_IN_GPIO GPIO_NUM_0

extern float ultrasonic_data;
extern struct timeval sleep_enter_time;

void ultrasonic_init(void);
void deepsleep(uint32_t sleep_time_sec);

float read_filtered_ultrasonic_distance(void);
uint8_t read_battery_percentage(void);
uint8_t read_solar_voltage(void);
void power_lora_only_on(void);
void power_peripherals_on(void);
void power_peripherals_off(void);

#endif

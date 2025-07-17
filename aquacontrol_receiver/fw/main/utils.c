#include "utils.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "rc_gpio.h"
#include "esp_timer.h"  // for esp_timer_get_time()

int64_t last_manual_override_time_us = 0;  // Time in microseconds
char *TAG = "RECEIVER_V1";

float ultrasonic_data = 0.0f;  // define here
uint32_t tick = 0;             // define here
float TANK_SIZE_CM = 140.0f;   // define here

float water_level_percentage;

void process_lora_data(float empty_space)
{
    float water_level = TANK_SIZE_CM - empty_space;
    water_level_percentage = (water_level * 100) / TANK_SIZE_CM;

    ESP_LOGI(TAG, "Empty Space: %.2f cm", empty_space);
    ESP_LOGI(TAG, "Water Level: %.2f cm", water_level);
    ESP_LOGI(TAG, "Water Level Percentage: %.2f", water_level_percentage);

    int water_level_percent_int = (int) water_level_percentage;
    int64_t current_time_us = esp_timer_get_time();

    // Check if 5 minutes (300 seconds) have passed since last manual override
    if ((current_time_us - last_manual_override_time_us) < 150000000) {
        ESP_LOGI(TAG, "Bypassing auto-control (manual override active)");
        return;
    }
    if(water_level_percent_int>=85)
    {
        gpio_set_level(RELAY_GPIO, 1);
        ESP_LOGI(TAG, "relay off");
    }
    else if (water_level_percent_int <= 30)
    {       
        gpio_set_level(RELAY_GPIO, 1);
        ESP_LOGI(TAG, "relay on");
    }
}


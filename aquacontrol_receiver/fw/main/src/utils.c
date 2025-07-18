#include "utils.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "rc_gpio.h"
#include "esp_timer.h"  // for esp_timer_get_time()
#include "supabase.h"

int64_t last_manual_override_time_us = 0;  // Time in microseconds
char *TAG = "RECEIVER";
int water_level_percent_int;

float ultrasonic_data = 0.0f;  // define here
uint32_t tick = 0;             // define here


float water_level_percentage;

void process_lora_data(float empty_space)
{
    float water_level = tank_size_cm - empty_space;
    water_level_percentage = (water_level * 100) / tank_size_cm;

    ESP_LOGI(TAG, "Empty Space: %.2f cm", empty_space);
    ESP_LOGI(TAG, "Water Level: %.2f cm", water_level);
    ESP_LOGI(TAG, "Water Level Percentage: %.2f", water_level_percentage);

    water_level_percent_int = (int) water_level_percentage;
    int64_t current_time_us = esp_timer_get_time();

    // Check if 5 minutes (300 seconds) have passed since last manual override
    // if ((current_time_us - last_manual_override_time_us) < 120000000) {
    //     ESP_LOGI(TAG, "Bypassing auto-control (manual override active)");
    //     return;
    // }
    if(auto_control)
    {
        if(water_level_percent_int>=auto_off_level && auto_off)
        {
            relay_off();
        }
        else if (water_level_percent_int <= auto_on_level && auto_on)
        {       
            relay_on();
        }
    }
    else
    {
        ESP_LOGW(TAG, "AUTO CONTROL IS OFF");
    }
}

void control_relay(int motor_control)
{
    if(motor_control)
    {
        relay_on();
    }
    else
    {
        relay_off();
    }
}

void relay_on()
{ 
    gpio_set_level(RELAY_GPIO, 1);
    ESP_LOGI(TAG, "relay on");
    update_control_state_from_esp32(1);
}

void relay_off()
{
    gpio_set_level(RELAY_GPIO, 0);
    ESP_LOGI(TAG, "relay off");
    update_control_state_from_esp32(0);
}
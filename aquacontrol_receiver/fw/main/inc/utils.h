#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <esp_log.h>

extern char *TAG;
// Declare variables, do NOT define them here
extern float ultrasonic_data;
extern uint32_t tick;
extern int water_level_percent_int;

extern int64_t last_manual_override_time_us;  // Declare to access from utils.c
// Function declaration
void process_lora_data(float empty_space);
void process_gpios();
void relay_on();
void relay_off();
void control_relay(int motor_control);

#endif // __UTILS_H__

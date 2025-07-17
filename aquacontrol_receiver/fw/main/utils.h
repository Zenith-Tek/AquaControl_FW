#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <esp_log.h>

// Declare variables, do NOT define them here
extern float ultrasonic_data;
extern uint32_t tick;

extern float TANK_SIZE_CM;  // use extern for variables defined elsewhere
extern int64_t last_manual_override_time_us;  // Declare to access from utils.c
// Function declaration
void process_lora_data(float empty_space);
void process_gpios();
void relay_on();
void relay_off();

#endif // __UTILS_H__

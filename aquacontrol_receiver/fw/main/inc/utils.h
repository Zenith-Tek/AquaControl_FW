#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <esp_log.h>

extern char *TAG;
// Declare variables, do NOT define them here
extern float ultrasonic_data;
extern uint32_t tick;
extern int water_level_percent_int;

#include <stdbool.h>

extern int64_t last_manual_override_time_us;  // Declare to access from utils.c
extern int64_t last_lora_recv_time_ms;
#define LORA_FAILSAFE_TIMEOUT_MS (5 * 60 * 1000) // 5 minutes

#define MAX_BINDINGS 8

typedef struct __attribute__((packed)) {
    uint8_t mac[6];
    char passcode[8];
    uint32_t last_seq_num;
} sender_binding_t;

typedef struct __attribute__((packed)) {
    int32_t count;
    sender_binding_t entries[MAX_BINDINGS];
} binding_table_t;

extern binding_table_t g_binding_table;

void load_bindings_from_nvs();
void save_bindings_to_nvs();
bool is_sender_authorized(const uint8_t *mac);

extern int rx_battery_percent;
extern float rx_solar_voltage;
extern char rx_alert_message[128];

void initialize_sntp(void);

// Function declaration
void process_lora_data(float empty_space, uint8_t battery_percent, uint8_t solar_voltage_x10);
void process_gpios();
void relay_on();
void relay_off();
void control_relay(int motor_control);

#endif // __UTILS_H__

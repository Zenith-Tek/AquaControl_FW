#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_http_client.h"
#include "esp_mac.h" 


/* ==================================================================== */
/* ==================== CONFIGURATION ================================= */
/* ==================================================================== */

// --- Supabase Configuration ---
// These values are taken directly from your Supabase project settings.
#define SUPABASE_URL "https://nkymflkrwqwjyleburxo.supabase.co"
#define SUPABASE_ANON_KEY "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im5reW1mbGtyd3F3anlsZWJ1cnhvIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTI2NTcyNjcsImV4cCI6MjA2ODIzMzI2N30.6j_JRwjTcTm6hNZ2uuNhc_XBWqyhNdcr4fX798kqvN0"
#define SUPABASE_TABLE_NAME "sensor_data"
extern int motor_state;
extern int auto_control;
extern int auto_on;
extern int auto_off;
extern int tank_size_cm;
extern int auto_on_level;
extern int auto_off_level;
void send_data_to_supabase();
void read_control_state_from_supabase();
void update_control_state_from_esp32(int new_state);
void sync_sender_bindings_from_supabase();
void sensor_data_task(void *pvParameters);
void device_control_task(void *pvParameters);
void get_mac_address(char *mac_str);
esp_err_t supabase_post_rpc(const char *function_name, const char *body);
void check_ota_updates_from_supabase();
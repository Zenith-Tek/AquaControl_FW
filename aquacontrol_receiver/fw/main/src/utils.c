#include "utils.h"
#include "wifi.h"
#include <stdio.h>
#include <time.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "rc_gpio.h"
#include "esp_timer.h"  // for esp_timer_get_time()
#include "supabase.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_sntp.h"

binding_table_t g_binding_table;

int64_t last_manual_override_time_us = 0;  // Time in microseconds
int64_t last_lora_recv_time_ms = 0;
char *TAG = "RECEIVER";
int water_level_percent_int;
int rx_battery_percent = 100;
float rx_solar_voltage = 0.0f;
char rx_alert_message[128] = "Normal";

float ultrasonic_data = 0.0f;  // define here
uint32_t tick = 0;             // define here


float water_level_percentage;

void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

void process_lora_data(float empty_space, uint8_t battery_percent, uint8_t solar_voltage_x10)
{
    if (battery_percent < 10) {
        snprintf(rx_alert_message, sizeof(rx_alert_message), "Sender battery critically low (%d%%)! Main process shutdown.", battery_percent);
        rx_battery_percent = battery_percent;
        rx_solar_voltage = (float)solar_voltage_x10 / 10.0f;
        
        ESP_LOGE(TAG, "EMERGENCY: Sender battery is %d%%! Turning relay OFF and uploading telemetry.", battery_percent);
        relay_off();
        
        if (supabase_is_online()) {
            send_data_to_supabase();
        }
        return;
    }

    float water_level = tank_size_cm - empty_space;
    water_level_percentage = (water_level * 100) / tank_size_cm;

    ESP_LOGI(TAG, "Empty Space: %.2f cm", empty_space);
    ESP_LOGI(TAG, "Water Level: %.2f cm", water_level);
    ESP_LOGI(TAG, "Water Level Percentage: %.2f", water_level_percentage);
    ESP_LOGI(TAG, "Tank Size : %d", tank_size_cm);
    water_level_percent_int = (int) water_level_percentage;

    // Save telemetry to globals
    rx_battery_percent = battery_percent;
    rx_solar_voltage = (float)solar_voltage_x10 / 10.0f;

    // Solar diagnostics logic using SNTP
    time_t now_epoch;
    struct tm timeinfo;
    time(&now_epoch);
    localtime_r(&now_epoch, &timeinfo);
    bool time_synced = (timeinfo.tm_year > 120);

    nvs_handle_t my_handle;
    int64_t last_solar_charge_epoch = 0;
    if (nvs_open("storage", NVS_READWRITE, &my_handle) == ESP_OK) {
        nvs_get_i64(my_handle, "sol_epoch", &last_solar_charge_epoch);
        
        if (time_synced) {
            // Is it daytime (8 AM to 5 PM)?
            if (timeinfo.tm_hour >= 8 && timeinfo.tm_hour < 17) {
                if (rx_solar_voltage >= 2.0f) {
                    last_solar_charge_epoch = (int64_t)now_epoch;
                    nvs_set_i64(my_handle, "sol_epoch", last_solar_charge_epoch);
                    nvs_commit(my_handle);
                    ESP_LOGI(TAG, "Daytime solar charging active. Saved epoch: %lld", last_solar_charge_epoch);
                }
            }

            if (last_solar_charge_epoch > 0) {
                int64_t elapsed_seconds = (int64_t)now_epoch - last_solar_charge_epoch;
                if (elapsed_seconds > 20 * 24 * 3600) {
                    snprintf(rx_alert_message, sizeof(rx_alert_message), "Contact customer care (solar hardware failure)");
                } else if (elapsed_seconds > 7 * 24 * 3600) {
                    snprintf(rx_alert_message, sizeof(rx_alert_message), "Clean solar panel (dust/debris detected)");
                } else {
                    snprintf(rx_alert_message, sizeof(rx_alert_message), "Normal");
                }
                ESP_LOGI(TAG, "Solar elapsed time: %lld seconds. Status: %s", elapsed_seconds, rx_alert_message);
            } else {
                last_solar_charge_epoch = (int64_t)now_epoch;
                nvs_set_i64(my_handle, "sol_epoch", last_solar_charge_epoch);
                nvs_commit(my_handle);
                snprintf(rx_alert_message, sizeof(rx_alert_message), "Normal");
            }
        } else {
            ESP_LOGW(TAG, "Time not synced yet, skipping solar elapsed diagnostics");
        }
        nvs_close(my_handle);
    }

    if (water_level_percent_int >= 0) {
        if (supabase_is_online()) {
            send_data_to_supabase();
        }
        
        if (water_level_percent_int >= 100) {
            ESP_LOGW(TAG, "Tank is 100%% full! Forcing Relay OFF for safety.");
            relay_off();
        }
        else {
            int64_t current_time_us = esp_timer_get_time();
            // 1-hour override window (3600 seconds)
            bool manual_override_active = ((current_time_us - last_manual_override_time_us) < 3600000000LL) && (last_manual_override_time_us != 0);

            if (manual_override_active) {
                ESP_LOGI(TAG, "Manual Override is active. Bypassing automatic controls.");
            }
            else {
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
        }
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

void load_bindings_from_nvs()
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err == ESP_OK) {
        size_t required_size = sizeof(binding_table_t);
        err = nvs_get_blob(my_handle, "bindings", &g_binding_table, &required_size);
        nvs_close(my_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Loaded %d LoRa bindings from NVS.", (int)g_binding_table.count);
            for (int i = 0; i < g_binding_table.count; i++) {
                ESP_LOGI(TAG, "Binding [%d]: MAC %02X:%02X:%02X:%02X:%02X:%02X, Passcode: %s, Seq: %lu",
                         i,
                         g_binding_table.entries[i].mac[0],
                         g_binding_table.entries[i].mac[1],
                         g_binding_table.entries[i].mac[2],
                         g_binding_table.entries[i].mac[3],
                         g_binding_table.entries[i].mac[4],
                         g_binding_table.entries[i].mac[5],
                         g_binding_table.entries[i].passcode,
                         (unsigned long)g_binding_table.entries[i].last_seq_num);
            }
            return;
        }
    }
    // If not found or error, initialize to empty
    g_binding_table.count = 0;
    ESP_LOGI(TAG, "No LoRa bindings found in NVS. Starting empty.");
}

void save_bindings_to_nvs()
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(my_handle, "bindings", &g_binding_table, sizeof(binding_table_t));
        if (err == ESP_OK) {
            err = nvs_commit(my_handle);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Successfully saved %d bindings to NVS.", (int)g_binding_table.count);
            } else {
                ESP_LOGE(TAG, "Failed to commit NVS write: %s", esp_err_to_name(err));
            }
        } else {
            ESP_LOGE(TAG, "Failed to write bindings blob to NVS: %s", esp_err_to_name(err));
        }
        nvs_close(my_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
    }
}

bool is_sender_authorized(const uint8_t *mac)
{
    for (int i = 0; i < g_binding_table.count; i++) {
        if (memcmp(g_binding_table.entries[i].mac, mac, 6) == 0) {
            return true;
        }
    }
    return false;
}
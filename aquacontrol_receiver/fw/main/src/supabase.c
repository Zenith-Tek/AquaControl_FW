#include "supabase.h"
#include "utils.h"
#define TAG_DB "Supabase"

int motor_state;
int auto_control;
int auto_on;
int auto_off;
int tank_size_cm;
int auto_on_level;
int auto_off_level;
int prev_motor_state = 0;
// --- Function to send data to Supabase ---
void send_data_to_supabase()
{
    // --- NEW: Get the device's MAC address to use as a unique ID ---
    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac); // Gets the factory-programmed MAC address

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    // The 'mac_str' buffer now contains the MAC address as a string

    // 1. Create a JSON object with your data
    cJSON *root = cJSON_CreateObject();

    // --- MODIFIED: Use the MAC address string as the device_id ---
    cJSON_AddStringToObject(root, "device_id", mac_str);
    cJSON_AddNumberToObject(root, "Water_Level", water_level_percent_int);


    char *json_string = cJSON_PrintUnformatted(root);
    ESP_LOGI(TAG_DB, "JSON Payload: %s", json_string);

    // 2. Configure the HTTP client
    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/%s", SUPABASE_URL, SUPABASE_TABLE_NAME);

    char bearer[256];
    snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // 3. Set headers required by Supabase
    esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Authorization", bearer);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Prefer", "return=minimal");

    // 4. Set the JSON payload for the POST request
    esp_http_client_set_post_field(client, json_string, strlen(json_string));

    // 5. Perform the request and check the result
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG_DB, "HTTP POST request successful, status code = %d", status_code);
        if (status_code == 201) {
             ESP_LOGI(TAG_DB, "Data successfully inserted into Supabase.");
        } else {
             ESP_LOGW(TAG_DB, "Data sent, but Supabase returned status: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG_DB, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    // 6. Clean up resources
    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(json_string);
}

// --- Function to READ control data FROM Supabase ---
// --- Function to READ control data FROM Supabase ---
void read_control_state_from_supabase() 
{
    ESP_LOGI(TAG_DB, "Reading control state from Supabase...");

    char url[256];
    snprintf(url, sizeof(url),
         "%s/rest/v1/device_control?select=motor_state,auto_control_enabled,auto_on_enabled,auto_off_enabled,tank_size_cm,auto_on_level,auto_off_level&id=eq.1",
         SUPABASE_URL);



    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .buffer_size_tx = 1024, // <--- THIS IS THE FIX! It was missing again.
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set headers
    esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
    char bearer[256];
    snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Authorization", bearer);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_DB, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }

    esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG_DB, "HTTP GET request finished, status code = %d", status_code);

    if (status_code == 200) {
        char response_buffer[256] = {0};
        int read_len = esp_http_client_read_response(client, response_buffer, sizeof(response_buffer) - 1);
        if (read_len > 0) {
            ESP_LOGI(TAG_DB, "HTTP GET Response Body: %s", response_buffer);

            cJSON *root = cJSON_Parse(response_buffer);
            if (cJSON_IsArray(root) && cJSON_GetArraySize(root) > 0) {
                cJSON *first_element = cJSON_GetArrayItem(root, 0);
                cJSON *motor_state_json = cJSON_GetObjectItem(first_element, "motor_state");
                cJSON *auto_control_json = cJSON_GetObjectItem(first_element, "auto_control_enabled");
                cJSON *auto_on_json = cJSON_GetObjectItem(first_element, "auto_on_enabled");
                cJSON *auto_off_json = cJSON_GetObjectItem(first_element, "auto_off_enabled");
                cJSON *tank_size_cm_json = cJSON_GetObjectItem(first_element, "tank_size_cm");
                cJSON *auto_on_level_json = cJSON_GetObjectItem(first_element, "auto_on_level");
                cJSON *auto_off_level_json = cJSON_GetObjectItem(first_element, "auto_off_level");

               if (cJSON_IsBool(motor_state_json) &&
                    cJSON_IsBool(auto_control_json) &&
                    cJSON_IsBool(auto_on_json) &&
                    cJSON_IsBool(auto_off_json)) {
                    
                    motor_state = cJSON_IsTrue(motor_state_json) ? 1 : 0;
                    auto_control = cJSON_IsTrue(auto_control_json) ? 1 : 0;
                    auto_on = cJSON_IsTrue(auto_on_json) ? 1 : 0;
                    auto_off = cJSON_IsTrue(auto_off_json) ? 1 : 0;

                    ESP_LOGW(TAG_DB, ">>>>>>>>> CURRENT DEVICE STATE <<<<<<<<<");
                    ESP_LOGW(TAG_DB, "Motor State           : %d", motor_state);
                    ESP_LOGW(TAG_DB, "Auto Control Enabled  : %d", auto_control);
                    ESP_LOGW(TAG_DB, "Auto ON Enabled       : %d", auto_on);
                    ESP_LOGW(TAG_DB, "Auto OFF Enabled      : %d", auto_off);

                    if (cJSON_IsNumber(tank_size_cm_json)) {
                        tank_size_cm = (int)tank_size_cm_json->valuedouble;
                        ESP_LOGW(TAG_DB, "Tank Size (cm)        : %d", tank_size_cm);
                    }
                    else {
                        ESP_LOGW(TAG_DB, "Tank Size (cm)        : Not Set");
                    }
                    
                    if (cJSON_IsNumber(auto_on_level_json)) {
                        auto_on_level = (int)auto_on_level_json->valuedouble;
                        ESP_LOGW(TAG_DB, "auto_on_level        : %d", auto_on_level);
                    }
                    else {
                        ESP_LOGW(TAG_DB, "auto_on_level        : Not Set");
                    }

                    if (cJSON_IsNumber(auto_off_level_json)) {
                        auto_off_level = (int)auto_off_level_json->valuedouble;
                        ESP_LOGW(TAG_DB, "auto_off_level        : %d", auto_off_level);
                    }
                    else {
                        ESP_LOGW(TAG_DB, "auto_off_level        : Not Set");
                    }
                    
                    
                }

            } else {
                ESP_LOGE(TAG_DB, "Failed to parse JSON or JSON array is empty.");
            }
            cJSON_Delete(root);
        } else {
            ESP_LOGE(TAG_DB, "Failed to read response body, read_len = %d", read_len);
        }
    } else {
        ESP_LOGE(TAG_DB, "Supabase returned a non-200 status code.");
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if(prev_motor_state == motor_state)
    {
        continue;
    }
    if(prev_motor_state != motor_state)
    {
        prev_motor_state = motor_state;
        control_relay(prev_motor_state);
    }
}


// --- Function to UPDATE control data ON Supabase ---
void update_control_state_from_esp32(int new_state)
{
    ESP_LOGI(TAG_DB, "Updating control state on Supabase to %d", new_state);

    // 1. Create JSON payload: {"led_state": <new_state>}
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "motor_state", new_state);
    char *json_string = cJSON_PrintUnformatted(root);

    // 2. Configure the HTTP client
    char url[256];
    // This URL targets the specific row where id=1
    snprintf(url, sizeof(url), "%s/rest/v1/device_control?id=eq.1", SUPABASE_URL);

    char bearer[256];
    snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_PATCH, // PATCH is used for updating
        .timeout_ms = 10000,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // 3. Set headers
    esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
    esp_http_client_set_header(client, "Authorization", bearer);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // 4. Set the JSON payload
    esp_http_client_set_post_field(client, json_string, strlen(json_string));

    // 5. Perform the request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        // A successful PATCH usually returns 204 No Content
        if (status_code == 200 || status_code == 204) {
            ESP_LOGI(TAG_DB, "Successfully updated device state on Supabase.");
        } else {
            ESP_LOGE(TAG_DB, "Supabase update failed with status: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG_DB, "HTTP PATCH request failed: %s", esp_err_to_name(err));
    }

    // 6. Clean up
    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(json_string);
}

/**
 * @brief Task to handle device control state.
 * Reads state every 1 second.
 */
void device_control_task(void *pvParameters)
{
    ESP_LOGI(TAG_DB, "Device Control Task started.");

    // One-time test to demonstrate the ESP32 updating the state,
    // just like we had before.
    vTaskDelay(5000 / portTICK_PERIOD_MS); // Wait 5s before first update
    ESP_LOGI(TAG_DB, "[Control Task] Demonstrating ESP32 updating state to 1...");
    // update_control_state_from_esp32(1);
    
    while(1) {
        ESP_LOGI(TAG_DB, "[Control Task] Reading device control state...");
        read_control_state_from_supabase();
        
        // Wait for 1 second before the next run
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


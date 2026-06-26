#include "supabase.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "ota.h"
#include "utils.h"
#include "wifi.h"
#include <sys/time.h>
#include <time.h>
#define TAG_DB "Supabase"

int motor_state;
int auto_control;
volatile bool g_sender_ble_enable_req = false;
int auto_on;
int auto_off;
int tank_size_cm;
int auto_on_level;
int auto_off_level;
int prev_motor_state = 0;
// --- Function to send data to Supabase ---
void send_data_to_supabase() {
  // Get the device's MAC address to use as a unique ID
  char mac_str[18];
  get_mac_address(mac_str);

  // 1. Create a JSON object with your data
  cJSON *root = cJSON_CreateObject();

  extern int rx_battery_percent;
  extern float rx_solar_voltage;
  extern char rx_alert_message[128];

  // --- MODIFIED: Use the MAC address string as the device_id ---
  cJSON_AddStringToObject(root, "device_id", mac_str);
  cJSON_AddNumberToObject(root, "Water_Level", water_level_percent_int);
  cJSON_AddNumberToObject(root, "battery_percent", rx_battery_percent);
  cJSON_AddNumberToObject(root, "solar_voltage", rx_solar_voltage);
  cJSON_AddStringToObject(root, "alert_message", rx_alert_message);

  char *json_string = cJSON_PrintUnformatted(root);

  // 2. Configure the HTTP client
  char url[256];
  snprintf(url, sizeof(url), "%s/rest/v1/%s", SUPABASE_URL,
           SUPABASE_TABLE_NAME);

  char bearer[256];
  snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);

  esp_http_client_config_t config = {
      .url = url,
      .method = HTTP_METHOD_POST,
      .timeout_ms = 10000,
      .buffer_size = 4096,
      .buffer_size_tx = 4096,
      .crt_bundle_attach = esp_crt_bundle_attach,
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
    if (status_code == 201) {
      ESP_LOGI(TAG_DB, "[Sensor Task] Sensor data sent successfully");
    } else {
      ESP_LOGW(TAG_DB, "[Sensor Task] Data sent, but Supabase returned status: %d",
               status_code);
    }
  } else {
    ESP_LOGE(TAG_DB, "[Sensor Task] Failed to send sensor data: %s", esp_err_to_name(err));
  }

  // 6. Clean up resources
  esp_http_client_cleanup(client);
  cJSON_Delete(root);
  free(json_string);
}

// --- Function to READ control data FROM Supabase ---
// --- Function to READ control data FROM Supabase ---
void read_control_state_from_supabase() {
  char url[256];
  snprintf(
      url, sizeof(url),
      "%s/rest/v1/"
      "device_control?select=motor_state,auto_control_enabled,auto_on_enabled,"
      "auto_off_enabled,tank_size_cm,auto_on_level,auto_off_level,sender_ble_enable&id=eq.1",
      SUPABASE_URL);

  esp_http_client_config_t config = {
      .url = url,
      .method = HTTP_METHOD_GET,
      .timeout_ms = 10000,
      .buffer_size = 4096,
      .buffer_size_tx = 4096,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);

  // Set headers
  esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
  char bearer[256];
  snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
  esp_http_client_set_header(client, "Authorization", bearer);

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG_DB, "[Control Task] Failed to open HTTP connection for control state: %s",
             esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return;
  }

  esp_http_client_fetch_headers(client);
  int status_code = esp_http_client_get_status_code(client);

  if (status_code == 200) {
    char response_buffer[512] = {0};
    int read_len = esp_http_client_read_response(client, response_buffer,
                                                 sizeof(response_buffer) - 1);
    if (read_len > 0) {
      cJSON *root = cJSON_Parse(response_buffer);
      if (cJSON_IsArray(root) && cJSON_GetArraySize(root) > 0) {
        cJSON *first_element = cJSON_GetArrayItem(root, 0);
        cJSON *motor_state_json =
            cJSON_GetObjectItem(first_element, "motor_state");
        cJSON *auto_control_json =
            cJSON_GetObjectItem(first_element, "auto_control_enabled");
        cJSON *auto_on_json =
            cJSON_GetObjectItem(first_element, "auto_on_enabled");
        cJSON *auto_off_json =
            cJSON_GetObjectItem(first_element, "auto_off_enabled");
        cJSON *tank_size_cm_json =
            cJSON_GetObjectItem(first_element, "tank_size_cm");
        cJSON *auto_on_level_json =
            cJSON_GetObjectItem(first_element, "auto_on_level");
        cJSON *auto_off_level_json =
            cJSON_GetObjectItem(first_element, "auto_off_level");
        cJSON *sender_ble_enable_json =
            cJSON_GetObjectItem(first_element, "sender_ble_enable");

        if (cJSON_IsBool(motor_state_json) && cJSON_IsBool(auto_control_json) &&
            cJSON_IsBool(auto_on_json) && cJSON_IsBool(auto_off_json)) {

          motor_state = cJSON_IsTrue(motor_state_json) ? 1 : 0;
          auto_control = cJSON_IsTrue(auto_control_json) ? 1 : 0;
          auto_on = cJSON_IsTrue(auto_on_json) ? 1 : 0;
          auto_off = cJSON_IsTrue(auto_off_json) ? 1 : 0;

          if (cJSON_IsNumber(tank_size_cm_json)) tank_size_cm = (int)tank_size_cm_json->valuedouble;
          if (cJSON_IsNumber(auto_on_level_json)) auto_on_level = (int)auto_on_level_json->valuedouble;
          if (cJSON_IsNumber(auto_off_level_json)) auto_off_level = (int)auto_off_level_json->valuedouble;
          
          if (cJSON_IsBool(sender_ble_enable_json)) {
            g_sender_ble_enable_req = cJSON_IsTrue(sender_ble_enable_json);
          }

          ESP_LOGI(TAG_DB, "[Control Task] Fetched control state from Supabase (BLE Enable: %d)", g_sender_ble_enable_req);
        }

      } else {
        ESP_LOGE(TAG_DB, "[Control Task] Failed to parse JSON or JSON array is empty.");
      }
      cJSON_Delete(root);
    } else {
      ESP_LOGE(TAG_DB, "[Control Task] Failed to read response body, read_len = %d", read_len);
    }
  } else {
    ESP_LOGE(TAG_DB, "[Control Task] Failed to fetch control state, status = %d", status_code);
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  if (prev_motor_state != motor_state) {
    extern int64_t last_manual_override_time_us;
    last_manual_override_time_us = esp_timer_get_time();
    prev_motor_state = motor_state;
    control_relay(prev_motor_state);
  }
}

void update_control_state_from_esp32(int new_state) {
  if (!supabase_is_online()) {
    ESP_LOGI(TAG_DB, "[Control Task] Offline: skipping control state update to Supabase.");
    return;
  }

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
      .buffer_size = 4096,
      .buffer_size_tx = 4096,
      .crt_bundle_attach = esp_crt_bundle_attach,
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
      ESP_LOGI(TAG_DB, "[Control Task] Device state updated to %d in Supabase", new_state);
    } else {
      ESP_LOGE(TAG_DB, "[Control Task] Failed to update device state: status = %d", status_code);
    }
  } else {
    ESP_LOGE(TAG_DB, "[Control Task] Failed to update device state: %s", esp_err_to_name(err));
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
static int parse_mac(const char *mac_str, uint8_t *mac_bytes) {
  unsigned int mac[6];
  int parsed = sscanf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", &mac[0],
                      &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
  if (parsed == 6) {
    for (int i = 0; i < 6; i++) {
      mac_bytes[i] = (uint8_t)mac[i];
    }
    return 0;
  }
  parsed = sscanf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", &mac[0], &mac[1],
                  &mac[2], &mac[3], &mac[4], &mac[5]);
  if (parsed == 6) {
    for (int i = 0; i < 6; i++) {
      mac_bytes[i] = (uint8_t)mac[i];
    }
    return 0;
  }
  return -1;
}

void sync_sender_bindings_from_supabase() {
  uint8_t mac[6] = {0};
  esp_efuse_mac_get_default(mac);
  char mac_str[18];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],
           mac[1], mac[2], mac[3], mac[4], mac[5]);

  char url[320];
  snprintf(url, sizeof(url),
           "%s/rest/v1/"
           "sender_bindings?select=sender_mac,passcode&receiver_mac=eq.%s",
           SUPABASE_URL, mac_str);

  esp_http_client_config_t config = {
      .url = url,
      .method = HTTP_METHOD_GET,
      .timeout_ms = 10000,
      .buffer_size = 4096,
      .buffer_size_tx = 4096,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);

  // Set headers
  esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
  char bearer[256];
  snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
  esp_http_client_set_header(client, "Authorization", bearer);

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG_DB, "[Control Task] Failed to open HTTP connection for bindings: %s",
             esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return;
  }

  esp_http_client_fetch_headers(client);
  int status_code = esp_http_client_get_status_code(client);

  if (status_code == 200) {
    char response_buffer[1024] = {0};
    int read_len = esp_http_client_read_response(client, response_buffer,
                                                 sizeof(response_buffer) - 1);
    if (read_len > 0) {
      cJSON *root = cJSON_Parse(response_buffer);
      if (cJSON_IsArray(root)) {
        int size = cJSON_GetArraySize(root);
        if (size > MAX_BINDINGS)
          size = MAX_BINDINGS;

        binding_table_t new_table;
        new_table.count = 0;

        for (int i = 0; i < size; i++) {
          cJSON *item = cJSON_GetArrayItem(root, i);
          cJSON *sender_mac_json = cJSON_GetObjectItem(item, "sender_mac");
          cJSON *passcode_json = cJSON_GetObjectItem(item, "passcode");
          if (cJSON_IsString(sender_mac_json) &&
              cJSON_IsString(passcode_json)) {
            uint8_t sender_mac_bytes[6];
            if (parse_mac(sender_mac_json->valuestring, sender_mac_bytes) ==
                0) {
              memcpy(new_table.entries[new_table.count].mac, sender_mac_bytes,
                     6);
              strncpy(new_table.entries[new_table.count].passcode,
                      passcode_json->valuestring,
                      sizeof(new_table.entries[new_table.count].passcode) - 1);
              new_table.entries[new_table.count].passcode
                  [sizeof(new_table.entries[new_table.count].passcode) - 1] =
                  '\0';

              // Look up existing last_seq_num to preserve it
              uint32_t existing_seq = 0;
              for (int j = 0; j < g_binding_table.count; j++) {
                if (memcmp(g_binding_table.entries[j].mac, sender_mac_bytes,
                           6) == 0) {
                  existing_seq = g_binding_table.entries[j].last_seq_num;
                  break;
                }
              }
              new_table.entries[new_table.count].last_seq_num = existing_seq;
              new_table.count++;
            }
          }
        }

        bool changed = false;
        if (new_table.count != g_binding_table.count) {
          changed = true;
        } else {
          for (int i = 0; i < new_table.count; i++) {
            if (memcmp(g_binding_table.entries[i].mac, new_table.entries[i].mac,
                       6) != 0 ||
                strcmp(g_binding_table.entries[i].passcode,
                       new_table.entries[i].passcode) != 0) {
              changed = true;
              break;
            }
          }
        }

        if (changed) {
          ESP_LOGI(TAG_DB, "[Control Task] Bindings updated. Saving %d entries to NVS.",
                   (int)new_table.count);
          g_binding_table = new_table;
          save_bindings_to_nvs();
        } else {
          ESP_LOGI(TAG_DB, "[Control Task] Local bindings cache is up to date.");
        }
      }
      if (root) {
        cJSON_Delete(root);
      }
    }
  } else {
    ESP_LOGE(TAG_DB, "[Control Task] Failed to fetch bindings, status = %d", status_code);
  }

  esp_http_client_cleanup(client);
}

volatile bool g_ota_in_progress = false;

static esp_websocket_client_handle_t ws_client = NULL;
static bool ws_connected = false;

void supabase_stop_websocket(void) {
  if (ws_client) {
    ESP_LOGW(TAG_DB, "[Control Task] Stopping and destroying WebSocket client to free heap for OTA...");
    esp_websocket_client_stop(ws_client);
    esp_websocket_client_destroy(ws_client);
    ws_client = NULL;
    ws_connected = false;
  }
}

static void parse_realtime_payload(cJSON *payload) {
  if (!payload)
    return;
  cJSON *data = cJSON_GetObjectItem(payload, "data");
  if (!data)
    return;
  cJSON *record = cJSON_GetObjectItem(data, "record");
  if (!record)
    return;

  cJSON *motor_state_json = cJSON_GetObjectItem(record, "motor_state");
  if (motor_state_json && cJSON_IsBool(motor_state_json)) {
    motor_state = cJSON_IsTrue(motor_state_json) ? 1 : 0;
    ESP_LOGW(TAG_DB, "[Realtime Task] REALTIME: Motor State -> %d", motor_state);
    if (prev_motor_state != motor_state) {
      extern int64_t last_manual_override_time_us;
      last_manual_override_time_us = esp_timer_get_time();
      prev_motor_state = motor_state;
      control_relay(prev_motor_state);
    }
  }

  cJSON *auto_control_json =
      cJSON_GetObjectItem(record, "auto_control_enabled");
  if (auto_control_json && cJSON_IsBool(auto_control_json)) {
    auto_control = cJSON_IsTrue(auto_control_json) ? 1 : 0;
    ESP_LOGW(TAG_DB, "[Realtime Task] REALTIME: Auto Control Enabled -> %d", auto_control);
  }

  cJSON *auto_on_json = cJSON_GetObjectItem(record, "auto_on_enabled");
  if (auto_on_json && cJSON_IsBool(auto_on_json)) {
    auto_on = cJSON_IsTrue(auto_on_json) ? 1 : 0;
    ESP_LOGW(TAG_DB, "[Realtime Task] REALTIME: Auto ON Enabled -> %d", auto_on);
  }

  cJSON *auto_off_json = cJSON_GetObjectItem(record, "auto_off_enabled");
  if (auto_off_json && cJSON_IsBool(auto_off_json)) {
    auto_off = cJSON_IsTrue(auto_off_json) ? 1 : 0;
    ESP_LOGW(TAG_DB, "[Realtime Task] REALTIME: Auto OFF Enabled -> %d", auto_off);
  }

  cJSON *tank_size_cm_json = cJSON_GetObjectItem(record, "tank_size_cm");
  if (tank_size_cm_json && cJSON_IsNumber(tank_size_cm_json)) {
    tank_size_cm = (int)tank_size_cm_json->valuedouble;
    ESP_LOGW(TAG_DB, "[Realtime Task] REALTIME: Tank Size -> %d cm", tank_size_cm);
  }

  cJSON *auto_on_level_json = cJSON_GetObjectItem(record, "auto_on_level");
  if (auto_on_level_json && cJSON_IsNumber(auto_on_level_json)) {
    auto_on_level = (int)auto_on_level_json->valuedouble;
    ESP_LOGW(TAG_DB, "[Realtime Task] REALTIME: Auto ON Level -> %d", auto_on_level);
  }

  cJSON *auto_off_level_json = cJSON_GetObjectItem(record, "auto_off_level");
  if (auto_off_level_json && cJSON_IsNumber(auto_off_level_json)) {
    auto_off_level = (int)auto_off_level_json->valuedouble;
    ESP_LOGW(TAG_DB, "[Realtime Task] REALTIME: Auto OFF Level -> %d", auto_off_level);
  }

  cJSON *sender_ble_enable_json = cJSON_GetObjectItem(record, "sender_ble_enable");
  if (sender_ble_enable_json && cJSON_IsBool(sender_ble_enable_json)) {
    g_sender_ble_enable_req = cJSON_IsTrue(sender_ble_enable_json);
    ESP_LOGW(TAG_DB, "[Realtime Task] REALTIME: Sender BLE Enable -> %d", g_sender_ble_enable_req);
  }
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base,
                                    int32_t event_id, void *event_data) {
  esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
  switch (event_id) {
  case WEBSOCKET_EVENT_CONNECTED: {
    ESP_LOGI(TAG_DB, "[Realtime Task] WebSocket Connected. Joining Topics...");
    ws_connected = true;
    // Join the Phoenix channel for realtime database changes
    char join_msg[512];
    snprintf(join_msg, sizeof(join_msg),
             "{\"topic\":\"realtime:public:device_control\","
             "\"event\":\"phx_join\","
             "\"payload\":{\"config\":{\"postgres_changes\":[{\"event\":\"*\","
             "\"schema\":\"public\",\"table\":\"device_control\"}]}},"
             "\"ref\":\"1\"}");
    esp_websocket_client_send_text(data->client, join_msg, strlen(join_msg),
                                   portMAX_DELAY);
    break;
  }
  case WEBSOCKET_EVENT_DISCONNECTED:
    ESP_LOGE(TAG_DB, "[Realtime Task] WebSocket Disconnected");
    ws_connected = false;
    break;
  case WEBSOCKET_EVENT_DATA:
    if (data->op_code == 0x01 && data->data_ptr != NULL) { // Text frame
      // Skip fragmented messages - only process when we have the complete
      // payload
      if (data->payload_offset != 0 || data->data_len != data->payload_len) {
        ESP_LOGW(TAG_DB,
                 "[Realtime Task] WS fragmented msg: offset=%d, data_len=%d, payload_len=%d "
                 "(skipped)",
                 data->payload_offset, data->data_len, data->payload_len);
        break;
      }
      cJSON *root = cJSON_ParseWithLength(data->data_ptr, data->data_len);
      if (root) {
        cJSON *topic = cJSON_GetObjectItem(root, "topic");
        cJSON *event = cJSON_GetObjectItem(root, "event");
        cJSON *payload = cJSON_GetObjectItem(root, "payload");

        if (cJSON_IsString(topic) &&
            strcmp(topic->valuestring, "realtime:public:device_control") == 0) {
          if (cJSON_IsString(event) &&
              strcmp(event->valuestring, "postgres_changes") == 0) {
            parse_realtime_payload(payload);
          }
        }
        cJSON_Delete(root);
      }
    }
    break;
  case WEBSOCKET_EVENT_ERROR:
    ESP_LOGE(TAG_DB, "[Realtime Task] WebSocket Error!");
    break;
  }
}

void device_control_task(void *pvParameters) {
  ESP_LOGI(TAG_DB,
           "[Control Task] Device Control Task started. Waiting for Wi-Fi connection...");

  // Initialize last_lora_recv_time_ms to current boot time to give a fresh
  // grace period
  last_lora_recv_time_ms = esp_timer_get_time() / 1000;
  int64_t last_sync_time_ms = 0;
  int64_t last_ota_check_time_ms = 0;
  int64_t last_heartbeat_time_ms = 0;

  // Wait until WiFi is connected before starting WebSocket
  wifi_ap_record_t ap_info;
  while (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  ESP_LOGI(
      TAG_DB,
      "[Control Task] Wi-Fi connected. Initializing Supabase Realtime WebSocket client...");

  // Initialize WebSocket connection
  char ws_url[512];
  if (strncmp(SUPABASE_URL, "https://", 8) == 0) {
    snprintf(ws_url, sizeof(ws_url),
             "wss://%s/realtime/v1/websocket?apikey=%s&vsn=1.0.0",
             SUPABASE_URL + 8, SUPABASE_ANON_KEY);
  } else if (strncmp(SUPABASE_URL, "http://", 7) == 0) {
    snprintf(ws_url, sizeof(ws_url),
             "ws://%s/realtime/v1/websocket?apikey=%s&vsn=1.0.0",
             SUPABASE_URL + 7, SUPABASE_ANON_KEY);
  } else {
    snprintf(ws_url, sizeof(ws_url),
             "wss://%s/realtime/v1/websocket?apikey=%s&vsn=1.0.0", SUPABASE_URL,
             SUPABASE_ANON_KEY);
  }

  esp_websocket_client_config_t ws_cfg = {
      .uri = ws_url,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .buffer_size = 4096,
      .task_stack = 8192,
  };
  ws_client = esp_websocket_client_init(&ws_cfg);
  if (ws_client) {
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY,
                                  websocket_event_handler, NULL);
    esp_websocket_client_start(ws_client);
  } else {
    ESP_LOGE(TAG_DB, "[Control Task] Failed to initialize WebSocket client!");
  }

  // Perform an initial poll to get the current state
  read_control_state_from_supabase();

  while (1) {
    if (g_ota_in_progress) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }
    int64_t current_time_ms = esp_timer_get_time() / 1000;

    // WebSocket heartbeat (every 25 seconds)
    if (ws_connected && (current_time_ms - last_heartbeat_time_ms > 25000 ||
                         last_heartbeat_time_ms == 0)) {
      const char *hb = "{\"topic\":\"phoenix\",\"event\":\"heartbeat\","
                       "\"payload\":{},\"ref\":\"2\"}";
      if (ws_client && esp_websocket_client_is_connected(ws_client)) {
        esp_websocket_client_send_text(ws_client, hb, strlen(hb),
                                       portMAX_DELAY);
        last_heartbeat_time_ms = current_time_ms;
      }
    }

    // Periodic bindings sync: every 30 seconds
    if (current_time_ms - last_sync_time_ms > 30000 || last_sync_time_ms == 0) {
      sync_sender_bindings_from_supabase();
      last_sync_time_ms = current_time_ms;
    }

    // Periodic OTA Check: every 5 minutes
    if (current_time_ms - last_ota_check_time_ms > 300000 ||
        last_ota_check_time_ms == 0) {
      check_ota_updates_from_supabase();
      last_ota_check_time_ms = current_time_ms;
    }

    // Communication Fail-Safe: check if sender went offline while motor is
    // running
    if (motor_state == 1 &&
        (current_time_ms - last_lora_recv_time_ms) > LORA_FAILSAFE_TIMEOUT_MS) {
      ESP_LOGE(TAG_DB,
               "[Control Task] FAILSAFE: Communication lost with Sender! No packet for %lld "
               "seconds. Shutting down motor.",
               (current_time_ms - last_lora_recv_time_ms) / 1000);
      motor_state = 0;
      prev_motor_state = 0;
      relay_off();
    }

    // Wait for 1 second before the next run
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void get_mac_address(char *mac_str) {
  uint8_t mac[6] = {0};
  esp_efuse_mac_get_default(mac);
  snprintf(mac_str, 18, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
}

esp_err_t supabase_post_rpc(const char *function_name, const char *body) {
  if (!function_name || !body)
    return ESP_ERR_INVALID_ARG;

  char url[256];
  snprintf(url, sizeof(url), "%s/rest/v1/rpc/%s", SUPABASE_URL, function_name);

  char bearer[256];
  snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);

  esp_http_client_config_t config = {
      .url = url,
      .method = HTTP_METHOD_POST,
      .timeout_ms = 10000,
      .buffer_size = 4096,
      .buffer_size_tx = 4096,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client)
    return ESP_FAIL;

  esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
  esp_http_client_set_header(client, "Authorization", bearer);
  esp_http_client_set_header(client, "Content-Type", "application/json");

  esp_http_client_set_post_field(client, body, strlen(body));

  esp_err_t err = esp_http_client_perform(client);
  int status_code = 0;
  if (err == ESP_OK) {
    status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG_DB, "[RPC Task] RPC '%s' response status code = %d", function_name,
             status_code);
  } else {
    ESP_LOGE(TAG_DB, "[RPC Task] RPC '%s' request failed: %s", function_name,
             esp_err_to_name(err));
  }

  esp_http_client_cleanup(client);
  return (err == ESP_OK && status_code >= 200 && status_code < 300) ? ESP_OK
                                                                    : ESP_FAIL;
}

void check_ota_updates_from_supabase() {
  ESP_LOGI(TAG_DB, "[Control Task] Checking for OTA updates from Supabase...");

  char url[256];
  snprintf(url, sizeof(url),
           "%s/rest/v1/"
           "system_control?select=version,bin_url,target_device_id&id=eq.1",
           SUPABASE_URL);

  esp_http_client_config_t config = {
      .url = url,
      .method = HTTP_METHOD_GET,
      .timeout_ms = 10000,
      .buffer_size = 4096,
      .buffer_size_tx = 4096,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client)
    return;

  esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
  char bearer[256];
  snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);
  esp_http_client_set_header(client, "Authorization", bearer);

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    esp_http_client_cleanup(client);
    return;
  }

  esp_http_client_fetch_headers(client);
  int status_code = esp_http_client_get_status_code(client);
  if (status_code == 200) {
    char response_buffer[512] = {0};
    int read_len = esp_http_client_read_response(client, response_buffer,
                                                 sizeof(response_buffer) - 1);
    if (read_len > 0) {
      cJSON *root = cJSON_Parse(response_buffer);
      if (cJSON_IsArray(root) && cJSON_GetArraySize(root) > 0) {
        cJSON *first_element = cJSON_GetArrayItem(root, 0);
        ota_handle_system_control_record(first_element);
      }
      cJSON_Delete(root);
    }
  }
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
}

void send_heartbeat_to_supabase(void) {
  // Get the device's MAC address to use as device_id
  char mac_str[18];
  get_mac_address(mac_str);

  // Get Wi-Fi RSSI
  int rssi = 0;
  wifi_ap_record_t ap_info;
  if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
    rssi = ap_info.rssi;
  }
  char current_ssid[33] = {0};
  wifi_config_t wifi_cfg;
  if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) == ESP_OK) {
    strncpy(current_ssid, (char *)wifi_cfg.sta.ssid, sizeof(current_ssid) - 1);
  }
  // Get uptime in seconds
  int64_t uptime_sec = esp_timer_get_time() / 1000000;

  // Get reboot reason
  extern const char *G_REBOOT_REASON_STR;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "device_id", mac_str);
  cJSON_AddNumberToObject(root, "wifi_rssi", rssi);
  cJSON_AddNumberToObject(root, "uptime_seconds", uptime_sec);
  cJSON_AddStringToObject(root, "firmware_version", APP_VERSION);
  cJSON_AddStringToObject(root, "last_reboot_reason",
                          G_REBOOT_REASON_STR ? G_REBOOT_REASON_STR
                                              : "Unknown");
  cJSON_AddNumberToObject(root, "device_timestamp", (int64_t)time(NULL));

  char *json_string = cJSON_PrintUnformatted(root);
  ESP_LOGI(TAG_DB, "[Heartbeat Task] Heartbeat Payload: %s", json_string);

  char url[256];
  snprintf(url, sizeof(url), "%s/rest/v1/device_status", SUPABASE_URL);

  char bearer[256];
  snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);

  esp_http_client_config_t config = {
      .url = url,
      .method = HTTP_METHOD_POST,
      .timeout_ms = 10000,
      .buffer_size = 4096,
      .buffer_size_tx = 4096,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    cJSON_Delete(root);
    free(json_string);
    return;
  }

  esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
  esp_http_client_set_header(client, "Authorization", bearer);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  // resolution=merge-duplicates maps to PostgREST upsert (ON CONFLICT DO
  // UPDATE)
  esp_http_client_set_header(client, "Prefer",
                             "resolution=merge-duplicates, return=minimal");

  esp_http_client_set_post_field(client, json_string, strlen(json_string));

  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK) {
    ESP_LOGI(TAG_DB, "[Heartbeat Task] Heartbeat sent successfully (SSID: %s)", current_ssid);
  } else {
    ESP_LOGE(TAG_DB, "[Heartbeat Task] Heartbeat failed: %s", esp_err_to_name(err));
  }

  esp_http_client_cleanup(client);
  cJSON_Delete(root);
  free(json_string);
}

TaskHandle_t heartbeat_task_handle = NULL;

void trigger_heartbeat_now(void) {
  if (heartbeat_task_handle != NULL) {
    xTaskNotifyGive(heartbeat_task_handle);
  }
}

void supabase_heartbeat_task(void *pvParameters) {
  heartbeat_task_handle = xTaskGetCurrentTaskHandle();
  ESP_LOGI(TAG_DB, "[Heartbeat Task] Heartbeat Task started.");

  // Initial delay before starting updates
  vTaskDelay(10000 / portTICK_PERIOD_MS);

  while (1) {
    if (g_ota_in_progress) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }
    if (supabase_is_online()) {
      ESP_LOGI(TAG_DB, "[Heartbeat Task] Updating device status...");
      send_heartbeat_to_supabase();
    } else {
      ESP_LOGW(TAG_DB, "[Heartbeat Task] Offline: skipping device status update.");
    }

    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(15000));
  }
}

void update_sender_ble_enable_in_supabase(bool enabled) {
  if (!supabase_is_online()) {
    ESP_LOGI(TAG_DB, "[Control Task] Offline: skipping BLE trigger status update.");
    return;
  }

  cJSON *root = cJSON_CreateObject();
  cJSON_AddBoolToObject(root, "sender_ble_enable", enabled);
  char *json_string = cJSON_PrintUnformatted(root);

  char url[256];
  snprintf(url, sizeof(url), "%s/rest/v1/device_control?id=eq.1", SUPABASE_URL);

  char bearer[256];
  snprintf(bearer, sizeof(bearer), "Bearer %s", SUPABASE_ANON_KEY);

  esp_http_client_config_t config = {
      .url = url,
      .method = HTTP_METHOD_PATCH,
      .timeout_ms = 10000,
      .buffer_size = 4096,
      .buffer_size_tx = 4096,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);

  esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
  esp_http_client_set_header(client, "Authorization", bearer);
  esp_http_client_set_header(client, "Content-Type", "application/json");

  esp_http_client_set_post_field(client, json_string, strlen(json_string));

  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK) {
    int status_code = esp_http_client_get_status_code(client);
    if (status_code == 200 || status_code == 204) {
      ESP_LOGI(TAG_DB, "[Control Task] Sender BLE Enable updated to %d in Supabase", enabled);
    } else {
      ESP_LOGE(TAG_DB, "[Control Task] Failed to update Sender BLE Enable: status = %d", status_code);
    }
  } else {
    ESP_LOGE(TAG_DB, "[Control Task] Failed to update Sender BLE Enable: %s", esp_err_to_name(err));
  }

  esp_http_client_cleanup(client);
  cJSON_Delete(root);
  free(json_string);
}

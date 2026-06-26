#include "ble_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "esp_mac.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define TAG_BLE "BLE_OTA"

// Custom GATT Service & Characteristic UUIDs
// Service UUID: AD751A20-2E24-4B7C-9F6F-E62D38B2E050 (written little-endian)
static ble_uuid128_t ota_service_uuid = BLE_UUID128_INIT(
    0x50, 0xe0, 0xb2, 0x38, 0x2d, 0xe6, 0x6f, 0x9f,
    0x7c, 0x4b, 0x24, 0x2e, 0x20, 0x1a, 0x75, 0xad
);

// Control Characteristic UUID: AD751A21-2E24-4B7C-9F6F-E62D38B2E050
static ble_uuid128_t ota_control_uuid = BLE_UUID128_INIT(
    0x50, 0xe0, 0xb2, 0x38, 0x2d, 0xe6, 0x6f, 0x9f,
    0x7c, 0x4b, 0x24, 0x2e, 0x21, 0x1a, 0x75, 0xad
);

// Data Characteristic UUID: AD751A22-2E24-4B7C-9F6F-E62D38B2E050
static ble_uuid128_t ota_data_uuid = BLE_UUID128_INIT(
    0x50, 0xe0, 0xb2, 0x38, 0x2d, 0xe6, 0x6f, 0x9f,
    0x7c, 0x4b, 0x24, 0x2e, 0x22, 0x1a, 0x75, 0xad
);

typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_WRITING,
    OTA_STATE_FINISHED,
    OTA_STATE_ERROR
} ota_state_t;

static ota_state_t g_ota_state = OTA_STATE_IDLE;
static esp_ota_handle_t g_update_handle = 0;
static const esp_partition_t *g_update_partition = NULL;
static volatile uint32_t g_inactivity_seconds = 0;
static uint32_t g_total_bytes_written = 0;

static uint16_t ota_control_handle;
static uint8_t ble_addr_type;

// Function declarations
static int ota_control_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ota_data_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static void handle_ota_control(uint8_t cmd);
static void handle_ota_data(const uint8_t *data, uint16_t len);
static void start_advertising(void);

// GATT Service registration structure
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &ota_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &ota_control_uuid.u,
                .access_cb = ota_control_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &ota_control_handle,
            },
            {
                .uuid = &ota_data_uuid.u,
                .access_cb = ota_data_cb,
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                0, // No more characteristics in this service
            }
        },
    },
    {
        0, // No more services
    },
};

static int ota_control_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len == 1) {
            uint8_t val;
            int rc = ble_hs_mbuf_to_flat(ctxt->om, &val, 1, &len);
            if (rc == 0) {
                handle_ota_control(val);
            }
        }
    }
    return 0;
}

static int ota_data_cb(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        uint8_t buffer[512];
        if (len <= sizeof(buffer)) {
            uint16_t out_len;
            int rc = ble_hs_mbuf_to_flat(ctxt->om, buffer, len, &out_len);
            if (rc == 0) {
                handle_ota_data(buffer, out_len);
            }
        } else {
            uint8_t *large_buf = malloc(len);
            if (large_buf) {
                uint16_t out_len;
                int rc = ble_hs_mbuf_to_flat(ctxt->om, large_buf, len, &out_len);
                if (rc == 0) {
                    handle_ota_data(large_buf, out_len);
                }
                free(large_buf);
            }
        }
    }
    return 0;
}

static void handle_ota_control(uint8_t cmd) {
    g_inactivity_seconds = 0; // Reset inactivity timeout
    
    if (cmd == 0x01) { // Start OTA
        ESP_LOGI(TAG_BLE, "OTA Start Command Received");
        if (g_ota_state != OTA_STATE_IDLE) {
            ESP_LOGE(TAG_BLE, "OTA already in progress or in error state");
            return;
        }
        g_update_partition = esp_ota_get_next_update_partition(NULL);
        if (g_update_partition == NULL) {
            ESP_LOGE(TAG_BLE, "Passive OTA partition not found!");
            g_ota_state = OTA_STATE_ERROR;
            return;
        }
        ESP_LOGI(TAG_BLE, "Writing to partition %s at offset 0x%lx",
                 g_update_partition->label, (unsigned long)g_update_partition->address);
        esp_err_t err = esp_ota_begin(g_update_partition, OTA_SIZE_UNKNOWN, &g_update_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_BLE, "esp_ota_begin failed: %s", esp_err_to_name(err));
            g_ota_state = OTA_STATE_ERROR;
            return;
        }
        g_ota_state = OTA_STATE_WRITING;
        g_total_bytes_written = 0;
    } 
    else if (cmd == 0x02) { // End / Commit OTA
        ESP_LOGI(TAG_BLE, "OTA End/Commit Command Received");
        if (g_ota_state != OTA_STATE_WRITING) {
            ESP_LOGE(TAG_BLE, "OTA not in writing state");
            return;
        }
        esp_err_t err = esp_ota_end(g_update_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_BLE, "esp_ota_end failed: %s", esp_err_to_name(err));
            g_ota_state = OTA_STATE_ERROR;
            return;
        }
        err = esp_ota_set_boot_partition(g_update_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_BLE, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
            g_ota_state = OTA_STATE_ERROR;
            return;
        }
        g_ota_state = OTA_STATE_FINISHED;
        ESP_LOGI(TAG_BLE, "OTA Complete! Total bytes: %lu. Rebooting...", (unsigned long)g_total_bytes_written);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } 
    else if (cmd == 0x03) { // Abort OTA
        ESP_LOGI(TAG_BLE, "OTA Abort Command Received");
        if (g_ota_state == OTA_STATE_WRITING) {
            esp_ota_end(g_update_handle);
        }
        g_ota_state = OTA_STATE_IDLE;
        g_total_bytes_written = 0;
    }
}

static void handle_ota_data(const uint8_t *data, uint16_t len) {
    g_inactivity_seconds = 0; // Reset inactivity timeout
    
    if (g_ota_state != OTA_STATE_WRITING) {
        ESP_LOGE(TAG_BLE, "Data received but OTA not in writing state!");
        return;
    }
    
    esp_err_t err = esp_ota_write(g_update_handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_BLE, "esp_ota_write failed: %s", esp_err_to_name(err));
        g_ota_state = OTA_STATE_ERROR;
        return;
    }
    g_total_bytes_written += len;
    
    static uint32_t last_progress_bytes = 0;
    if (g_total_bytes_written - last_progress_bytes >= 10240) {
        ESP_LOGI(TAG_BLE, "Written %lu bytes to OTA partition", (unsigned long)g_total_bytes_written);
        last_progress_bytes = g_total_bytes_written;
    }
}

static int ble_ota_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG_BLE, "BLE Client Connected: status=%d", event->connect.status);
            g_inactivity_seconds = 0;
            break;
            
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG_BLE, "BLE Client Disconnected: reason=%d. Resuming advertising...", event->disconnect.reason);
            g_inactivity_seconds = 0;
            start_advertising();
            break;
            
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG_BLE, "MTU size negotiated: %d bytes", event->mtu.value);
            break;
    }
    return 0;
}

static void start_advertising(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char name[32];
    snprintf(name, sizeof(name), "AC_SND_%02X%02X", mac[4], mac[5]);
    
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "Error setting advertisement fields; rc=%d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_ota_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "Error starting advertising; rc=%d", rc);
    } else {
        ESP_LOGI(TAG_BLE, "Advertising started as device: %s", name);
    }
}

static void ble_ota_on_sync(void) {
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    assert(rc == 0);
    start_advertising();
}

static void ble_ota_on_reset(int reason) {
    ESP_LOGE(TAG_BLE, "Resetting NimBLE host: reason=%d", reason);
}

static void ble_host_task(void *param) {
    ESP_LOGI(TAG_BLE, "NimBLE Host task running.");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void start_ble_ota_mode(void) {
    ESP_LOGI(TAG_BLE, "================================================");
    ESP_LOGI(TAG_BLE, "=== ENTERING BLE ON-DEMAND OTA UPDATE MODE ===");
    ESP_LOGI(TAG_BLE, "================================================");
    
    g_ota_state = OTA_STATE_IDLE;
    g_inactivity_seconds = 0;
    g_total_bytes_written = 0;
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_BLE, "Failed to initialize NimBLE: %s", esp_err_to_name(ret));
        return;
    }
    
    ble_hs_cfg.sync_cb = ble_ota_on_sync;
    ble_hs_cfg.reset_cb = ble_ota_on_reset;
    
    ble_svc_gap_init();
    ble_svc_gatt_init();
    
    int rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "ble_gatts_count_cfg failed; rc=%d", rc);
        return;
    }
    
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "ble_gatts_add_svcs failed; rc=%d", rc);
        return;
    }
    
    nimble_port_freertos_init(ble_host_task);
    
    // Inactivity check watchdog loop
    extern uint8_t rtc_ble_enable_pending;
    extern void deepsleep(uint32_t time_in_sec);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        g_inactivity_seconds++;
        
        if (g_inactivity_seconds >= 300) {
            ESP_LOGW(TAG_BLE, "No BLE activity for 5 minutes. Timing out OTA mode.");
            break;
        }
    }
    
    ESP_LOGI(TAG_BLE, "Shutting down BLE controller...");
    rc = nimble_port_stop();
    if (rc == 0) {
        nimble_port_deinit();
        ESP_LOGI(TAG_BLE, "BLE controller shut down successfully.");
    } else {
        ESP_LOGE(TAG_BLE, "Failed to stop NimBLE; rc=%d", rc);
    }
    
    // Clear flag and return to sleep
    rtc_ble_enable_pending = 0;
    ESP_LOGI(TAG_BLE, "Re-entering low-power sleep mode...");
    deepsleep(300);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

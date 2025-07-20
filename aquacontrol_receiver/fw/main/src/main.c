#include "main.h"
#include "wifi.h"
#include "supabase.h"
#include "zt_lora.h"

#define TAG_1 "Provisoning"
#define TAG_INFO "Provisoning"

struct timeval tv;
extern float ultrasonic_data;
extern uint32_t tick;

void app_main()
{
	print_system_memory_status();
    setup_gpios();
    connect_wifi();
    read_control_state_from_supabase();
    initialise_lora();
    xTaskCreate(&device_control_task, "Device Control Task", 8192, NULL, 5, NULL);
	xTaskCreate(&task_rx, "RX", 16384, NULL, 5, NULL);

}

void print_system_memory_status() 
{
    ESP_LOGI(TAG_INFO, "========== Chip Information ===========================================");
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    const char *chip_model;
    switch (chip_info.model) {
        case CHIP_ESP32: chip_model = "ESP32"; break;
        case CHIP_ESP32S2: chip_model = "ESP32-S2"; break;
        case CHIP_ESP32S3: chip_model = "ESP32-S3"; break;
        case CHIP_ESP32C3: chip_model = "ESP32-C3"; break;
        case CHIP_ESP32H2: chip_model = "ESP32-H2"; break;
        default: chip_model = "Unknown"; break;
    }
    ESP_LOGI(TAG_INFO, "Chip model: %s", chip_model);

    ESP_LOGI(TAG_INFO,"CPU cores: %d", chip_info.cores);
    ESP_LOGI(TAG_INFO,"Silicon revision: %d", chip_info.revision);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);  // <-- updated function
    ESP_LOGI(TAG_INFO,"Flash size: %lu MB", (unsigned long) flash_size / (1024 * 1024));

    const esp_app_desc_t *app_desc = esp_app_get_description();
    gettimeofday(&tv, NULL);
    ESP_LOGI(TAG_INFO, "========== Program Version ============================================");
    ESP_LOGI(TAG_INFO, "[APP] Name: %s", app_desc->project_name);
    
    ESP_LOGI(TAG_INFO, "[APP] Version: %s", APP_VERSION);
    ESP_LOGI(TAG_INFO, "[APP] Compile Date: %s", app_desc->date);
    ESP_LOGI(TAG_INFO, "[APP] Compile Time: %s", app_desc->time);
    ESP_LOGI(TAG_INFO, "========== Heap Information ===========================================");
    ESP_LOGI(TAG_INFO,"Total free heap: %lu bytes", (unsigned long) heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    ESP_LOGI(TAG_INFO,"Minimum free heap since boot: %lu bytes", (unsigned long) heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
    ESP_LOGI(TAG_INFO,"Internal RAM free: %lu bytes", (unsigned long) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    ESP_LOGI(TAG_INFO, "========== Stack Information ==========================================");
    ESP_LOGI(TAG_INFO,"Current task stack high water mark: %lu bytes", (unsigned long) uxTaskGetStackHighWaterMark(NULL));

    ESP_LOGI(TAG_INFO, "========== Flash Partition Information ================================");
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (part != NULL) {
        ESP_LOGI(TAG_INFO,"App partition size: %lu bytes", (unsigned long) part->size);
    } else {
        ESP_LOGI(TAG_INFO,"App partition not found!");
    }

    ESP_LOGI(TAG_INFO, "=======================================================================");
}
#include "main.h"

extern float ultrasonic_data; 
unsigned long elapsed_time = 0;
unsigned long start_time;
struct timeval tv;

static const char *TAG_1 = "LORA_SENDER";
#define TAG_INFO "APP"

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


void task_tx(void *pvParameters)
{
    ESP_LOGI(pcTaskGetName(NULL), "Start");
    uint8_t buf[256]; // Maximum Payload size of SX1276/77/78/79 is 255

    while (1) {
		elapsed_time = (esp_timer_get_time() - start_time) / 1000000; 
		ESP_LOGI(TAG_1, "Elapsed time: %lu seconds", elapsed_time);
		if (elapsed_time < 20) {
			TickType_t nowTick = xTaskGetTickCount();

			// Format the float data into the buffer
			int send_len = snprintf((char *)buf, sizeof(buf), 
									"Ultrasonic data: %.2f CM, @ Tick: %"PRIu32,
									ultrasonic_data, nowTick);

			// Send the packet over LoRa
			lora_send_packet(buf, send_len);
			ESP_LOGI(pcTaskGetName(NULL), "%d byte packet sent: %s", send_len, buf);

			// Check for packet loss
			int lost = lora_packet_lost();
			if (lost != 0) {
				ESP_LOGW(pcTaskGetName(NULL), "%d packets lost", lost);
			}

			// Delay before next transmission
			vTaskDelay(pdMS_TO_TICKS(5000));			

		}
		else
		{
			ESP_LOGI(TAG_1, "20 seconds elapsed, entering deep sleep for 40 sec...");
            deepsleep();
		}

    }
}

void app_main()
{
	print_system_memory_status();

	if (lora_init() == 0) {
		ESP_LOGE(pcTaskGetName(NULL), "Does not recognize the module");
		while(1) {
			vTaskDelay(1);
		}
	}

	ESP_LOGI(pcTaskGetName(NULL), "Frequency is 433MHz");
	lora_set_frequency(433e6); // 433MHz
	lora_enable_crc();

	int cr = 1;
	int bw = 7;
	int sf = 7;

	lora_set_coding_rate(cr);
	//lora_set_coding_rate(CONFIG_CODING_RATE);
	//cr = lora_get_coding_rate();
	ESP_LOGI(pcTaskGetName(NULL), "coding_rate=%d", cr);

	lora_set_bandwidth(bw);
	//lora_set_bandwidth(CONFIG_BANDWIDTH);
	//int bw = lora_get_bandwidth();
	ESP_LOGI(pcTaskGetName(NULL), "bandwidth=%d", bw);

	lora_set_spreading_factor(sf);
	//lora_set_spreading_factor(CONFIG_SF_RATE);
	//int sf = lora_get_spreading_factor();
	ESP_LOGI(pcTaskGetName(NULL), "spreading_factor=%d", sf);
	// ultrasonic_init();
	start_time = esp_timer_get_time();  // Start time in microseconds
    ultrasonic_init();
	xTaskCreate(&task_tx, "TX", 1024*3, NULL, 5, NULL);


}

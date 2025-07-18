/* The example of ESP-IDF
 *
 * This sample code is in the public domain.
 */

#include <main.h>
#include <supabase.h>
#include <provisioning.h>

#define TAG_1 "Provisoning"
#define TAG_INFO "Provisoning"

struct timeval tv;
const int WIFI_CONNECTED_EVENT = BIT0;
EventGroupHandle_t wifi_event_group;
unsigned char wifi_connection_status = 1;
extern float ultrasonic_data;
extern uint32_t tick;
int retry_count = 0;

void task_rx(void *pvParameters)
{
    ESP_LOGI(pcTaskGetName(NULL), "Start");
    uint8_t buf[256]; // Max payload size

    while (1) {
        lora_receive(); // Set to receive mode

        if (lora_received()) {
            int rxLen = lora_receive_packet(buf, sizeof(buf));
            buf[rxLen] = '\0'; // Null-terminate to safely use as string

            ESP_LOGI(pcTaskGetName(NULL), "%d byte packet received: [%s]", rxLen, buf);

            // Try to parse the received message
            int parsed = sscanf((char *)buf,
                                "Ultrasonic data: %f CM, @ Tick: %"SCNu32,
                                &ultrasonic_data, &tick);

            if (parsed == 2) {
                ESP_LOGI(pcTaskGetName(NULL), "Parsed -> Ultrasonic: %.2f CM @ Tick: %"PRIu32,
                         ultrasonic_data, tick);
            } else {
                ESP_LOGW(pcTaskGetName(NULL), "Failed to parse data!");
            }
			process_lora_data(ultrasonic_data);
			send_data_to_supabase();
        }

        vTaskDelay(1); // Avoid watchdog timer issues
    }
}

void app_main()
{
	print_system_memory_status();

    initialise_lora();
	
    setup_gpios();
    
	bool provision_status = false;
    /* Initialize NVS partition */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated
         * and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());

        /* Retry nvs_flash_init */
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    wifi_init();

    provision_status = provisioning_init();

    /* If device is not yet provisioned start provisioning service */
    if (!provision_status) {
        
        do_provisioning();
        /* Wait for Wi-Fi connection */
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, true, true, portMAX_DELAY);
    } else {
        ESP_LOGI(TAG_1, "Already provisioned, starting Wi-Fi STA");
        /* We don't need the manager as device is already provisioned,
         * so let's release it's resources */
        wifi_prov_mgr_deinit();
        /* Start Wi-Fi station */
        wifi_init_sta();
        /* Wait for Wi-Fi connection */
        ESP_LOGI(TAG_1,"WAITING FOR WIFI_CONNECTED_EVENT");
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, true, true, WIFI_CONNECTED_EVENT_TIMEOUT);
    }
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    while (netif == NULL || !esp_netif_is_netif_up(netif)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        
    }
    read_control_state_from_supabase();
    xTaskCreate(&device_control_task, "Device Control Task", 4096, NULL, 5, NULL);
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

void wifi_init_sta(void)
{
    /* Start Wi-Fi in station mode */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_init(void)
{
    /* Initialize TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Initialize the event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();

    /* Register our event handler for Wi-Fi, IP and Provisioning related events */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &prov_event_handler, NULL));

    /* Initialize Wi-Fi including netif with default config */
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

void initialise_lora()
{
    	if (lora_init() == 0) {
		ESP_LOGE(pcTaskGetName(NULL), "Does not recognize the module");
		ESP_LOGE(pcTaskGetName(NULL), "Retrying");
		while(1) {
			printf("...");
			if(retry_count > 200)
			{
				retry_count = 0;
				esp_restart();
			}
			
			if (lora_retry() == 1)
			{
				retry_count = 0;
				break;
			} 
			retry_count++;
			vTaskDelay(100);
		}
	}
	
	ESP_LOGE(pcTaskGetName(NULL), "Recognized the module");
	
#if CONFIG_433MHZ
	ESP_LOGI(pcTaskGetName(NULL), "Frequency is 433MHz");
	lora_set_frequency(433e6); // 433MHz
#elif CONFIG_866MHZ
	ESP_LOGI(pcTaskGetName(NULL), "Frequency is 866MHz");
	lora_set_frequency(866e6); // 866MHz
#elif CONFIG_915MHZ
	ESP_LOGI(pcTaskGetName(NULL), "Frequency is 915MHz");
	lora_set_frequency(915e6); // 915MHz
#elif CONFIG_OTHER
	ESP_LOGI(pcTaskGetName(NULL), "Frequency is %dMHz", CONFIG_OTHER_FREQUENCY);
	long frequency = CONFIG_OTHER_FREQUENCY * 1000000;
	lora_set_frequency(frequency);
#endif

	lora_enable_crc();

	int cr = 1;
	int bw = 7;
	int sf = 7;
#if CONFIG_ADVANCED
	cr = CONFIG_CODING_RATE;
	bw = CONFIG_BANDWIDTH;
	sf = CONFIG_SF_RATE;
#endif

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
}
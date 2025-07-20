#include "zt_lora.h"
#include "utils.h"

extern float ultrasonic_data;
int retry_count = 0;

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
			
        }

        vTaskDelay(1); // Avoid watchdog timer issues
    }
}

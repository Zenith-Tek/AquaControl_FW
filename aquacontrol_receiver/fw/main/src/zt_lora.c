#include "zt_lora.h"
#include "utils.h"
#include "lora_protocol.h"
#include "cJSON.h"
#include "mbedtls/sha256.h"
#include "mbedtls/gcm.h"
#include "esp_random.h"

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

    while (1) {
        lora_receive(); // Set to receive mode

        if (lora_received()) {
            secure_lora_packet_t packet;
            int rxLen = lora_receive_packet((uint8_t *)&packet, sizeof(packet));

            if (rxLen == sizeof(secure_lora_packet_t)) {
                // Find matching sender in the binding table
                sender_binding_t *entry = NULL;
                for (int i = 0; i < g_binding_table.count; i++) {
                    if (memcmp(g_binding_table.entries[i].mac, packet.src_mac, 6) == 0) {
                        entry = &g_binding_table.entries[i];
                        break;
                    }
                }

                if (entry != NULL) {
                    // Check replay protection: incoming sequence number must be strictly greater than last received
                    if (packet.seq_num <= entry->last_seq_num && entry->last_seq_num != 0) {
                        ESP_LOGW(pcTaskGetName(NULL), "Rejected replayed packet! MAC: %02X:%02X:%02X:%02X:%02X:%02X, Seq: %lu (last: %lu)",
                                 packet.src_mac[0], packet.src_mac[1], packet.src_mac[2],
                                 packet.src_mac[3], packet.src_mac[4], packet.src_mac[5],
                                 (unsigned long)packet.seq_num, (unsigned long)entry->last_seq_num);
                    } else {
                        // Derive the 256-bit AES key from the passcode using SHA-256
                        uint8_t aes_key[32];
                        mbedtls_sha256((const unsigned char *)entry->passcode, strlen(entry->passcode), aes_key, 0);

                        // Decrypt and authenticate the payload
                        secure_payload_t payload;
                        mbedtls_gcm_context gcm_ctx;
                        mbedtls_gcm_init(&gcm_ctx);
                        mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, aes_key, 256);

                        // Plaintext header is first 16 bytes: src_mac (6) + dest_mac (6) + seq_num (4)
                        int ret = mbedtls_gcm_auth_decrypt(&gcm_ctx, sizeof(secure_payload_t),
                                                           packet.iv, 12,
                                                           (const unsigned char *)&packet, 16,
                                                           packet.tag, 16,
                                                           packet.ciphertext, (unsigned char *)&payload);
                        mbedtls_gcm_free(&gcm_ctx);

                        if (ret == 0) {
                            ESP_LOGI(pcTaskGetName(NULL), "Decrypted packet from authorized MAC %02X:%02X:%02X:%02X:%02X:%02X, Seq: %lu, Distance: %.2f cm, Battery: %u%%, Solar: %.1fV",
                                     packet.src_mac[0], packet.src_mac[1], packet.src_mac[2],
                                     packet.src_mac[3], packet.src_mac[4], packet.src_mac[5],
                                     (unsigned long)packet.seq_num, payload.distance_cm, payload.battery_percent,
                                     (float)payload.solar_voltage_x10 / 10.0f);

                            // Update sequence counter and save to NVS (rate-limited to protect flash wear)
                            entry->last_seq_num = packet.seq_num;
                            static int64_t last_nvs_save_time = 0;
                            int64_t now_sec = esp_timer_get_time() / 1000000;
                            if (now_sec - last_nvs_save_time >= 60) {
                                save_bindings_to_nvs();
                                last_nvs_save_time = now_sec;
                            }

                            // Process parsed sensor measurements
                            last_lora_recv_time_ms = esp_timer_get_time() / 1000;
                            tick = packet.seq_num;
                            ultrasonic_data = payload.distance_cm;
                            
                            extern int motor_state;
                            extern int auto_control;
                            process_lora_data(payload.distance_cm, payload.battery_percent, payload.solar_voltage_x10);

                            // Check reboot reason from Sender
                            uint8_t sender_reboot_reason = (payload.flags >> 1) & 0x07;
                            if (sender_reboot_reason != 0) {
                                char sender_mac_str[18];
                                snprintf(sender_mac_str, sizeof(sender_mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                                         packet.src_mac[0], packet.src_mac[1], packet.src_mac[2],
                                         packet.src_mac[3], packet.src_mac[4], packet.src_mac[5]);
                                const char *reason_str = "Unknown";
                                switch (sender_reboot_reason) {
                                    case 1: reason_str = "SoftwareReset"; break;
                                    case 2: reason_str = "Panic"; break;
                                    case 3: reason_str = "Watchdog"; break;
                                    case 4: reason_str = "Brownout"; break;
                                    case 5: reason_str = "ExternalReset"; break;
                                    case 6: reason_str = "Other"; break;
                                }
                                ESP_LOGW(pcTaskGetName(NULL), "Sender %s reported reboot/crash: %s", sender_mac_str, reason_str);

                                // Upload Sender crash report to Supabase via report_crash RPC (asynchronous helper task)
                                cJSON *root_crash = cJSON_CreateObject();
                                cJSON_AddStringToObject(root_crash, "p_device_id", sender_mac_str);
                                cJSON_AddStringToObject(root_crash, "p_firmware_version", "Sender-FW");
                                cJSON_AddStringToObject(root_crash, "p_reboot_reason", reason_str);
                                cJSON_AddNullToObject(root_crash, "p_crashed_task");
                                cJSON_AddNullToObject(root_crash, "p_fault_pc");
                                cJSON_AddNullToObject(root_crash, "p_backtrace");
                                cJSON_AddNullToObject(root_crash, "p_uptime_before_crash");
                                char *body_crash = cJSON_PrintUnformatted(root_crash);
                                
                                void upload_sender_crash_task(void *param);
                                if (xTaskCreate(upload_sender_crash_task, "up_snd_crash", 4096, body_crash, 3, NULL) != pdPASS) {
                                    free(body_crash);
                                }
                                cJSON_Delete(root_crash);
                            }

                            // Prepare and Send Encrypted ACK
                            secure_ack_packet_t ack_pkt;
                            memset(&ack_pkt, 0, sizeof(ack_pkt));
                            
                            memcpy(ack_pkt.src_mac, packet.dest_mac, 6);
                            memcpy(ack_pkt.dest_mac, packet.src_mac, 6);
                            ack_pkt.seq_num = packet.seq_num;

                            secure_ack_payload_t ack_payload;
                            ack_payload.motor_state = (uint8_t)motor_state;
                            ack_payload.auto_control_enabled = (uint8_t)auto_control;

                            esp_fill_random(ack_pkt.iv, 12);

                            mbedtls_gcm_context gcm_ctx_ack;
                            mbedtls_gcm_init(&gcm_ctx_ack);
                            mbedtls_gcm_setkey(&gcm_ctx_ack, MBEDTLS_CIPHER_ID_AES, aes_key, 256);
                            
                            mbedtls_gcm_crypt_and_tag(&gcm_ctx_ack, MBEDTLS_GCM_ENCRYPT,
                                                      sizeof(secure_ack_payload_t),
                                                      ack_pkt.iv, 12,
                                                      (const unsigned char *)&ack_pkt, 16,
                                                      (const unsigned char *)&ack_payload, ack_pkt.ciphertext,
                                                      16, ack_pkt.tag);
                            mbedtls_gcm_free(&gcm_ctx_ack);

                            vTaskDelay(pdMS_TO_TICKS(10));
                            lora_send_packet((uint8_t *)&ack_pkt, sizeof(ack_pkt));
                            ESP_LOGI(pcTaskGetName(NULL), "Sent encrypted ACK -> Motor: %d, AutoControl: %d",
                                     ack_payload.motor_state, ack_payload.auto_control_enabled);
                        } else {
                            ESP_LOGE(pcTaskGetName(NULL), "GCM decryption/authentication failed! Packet tampered or wrong passcode.");
                        }
                    }
                } else {
                    ESP_LOGW(pcTaskGetName(NULL), "Rejected packet from unauthorized Sender MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                             packet.src_mac[0], packet.src_mac[1], packet.src_mac[2],
                             packet.src_mac[3], packet.src_mac[4], packet.src_mac[5]);
                }
            } else {
                ESP_LOGW(pcTaskGetName(NULL), "Invalid packet length received: %d (expected %d)", rxLen, (int)sizeof(secure_lora_packet_t));
            }
        }

        vTaskDelay(1); // Avoid watchdog timer issues
    }
}

void upload_sender_crash_task(void *param) {
    char *body = (char *)param;
    if (body) {
        extern esp_err_t supabase_post_rpc(const char *function_name, const char *body);
        supabase_post_rpc("report_crash", body);
        free(body);
    }
    vTaskDelete(NULL);
}

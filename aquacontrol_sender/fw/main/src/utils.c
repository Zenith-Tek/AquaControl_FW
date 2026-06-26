#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "utils.h"
#include "esp_adc/adc_oneshot.h"

float ultrasonic_data = 0.0f;
struct timeval sleep_enter_time;
static const char *TAG = "Utils";

void power_peripherals_on(void)
{
    // Configure power control pins as OUTPUT
    // Make sure pad hold is disabled so we can drive them
    gpio_hold_dis(LORA_PWR_GPIO);
    gpio_hold_dis(JSN_PWR_GPIO);

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LORA_PWR_GPIO) | (1ULL << JSN_PWR_GPIO),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Set LOW (0) to turn on P-channel MOSFETs
    gpio_set_level(LORA_PWR_GPIO, 0);
    gpio_set_level(JSN_PWR_GPIO, 0);
    ESP_LOGI(TAG, "Peripherals powered ON");
}

void power_lora_only_on(void)
{
    gpio_hold_dis(LORA_PWR_GPIO);
    gpio_hold_dis(JSN_PWR_GPIO);

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LORA_PWR_GPIO) | (1ULL << JSN_PWR_GPIO),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Set LOW (0) to turn on LoRa, HIGH (1) to keep JSN off
    gpio_set_level(LORA_PWR_GPIO, 0);
    gpio_set_level(JSN_PWR_GPIO, 1);
    ESP_LOGI(TAG, "LoRa powered ON, JSN powered OFF (Emergency Mode)");
}

void power_peripherals_off(void)
{
    // Set HIGH (1) to turn off P-channel MOSFETs
    gpio_set_level(LORA_PWR_GPIO, 1);
    gpio_set_level(JSN_PWR_GPIO, 1);

    // Enable pad hold so they stay HIGH during deep sleep
    gpio_hold_en(LORA_PWR_GPIO);
    gpio_hold_en(JSN_PWR_GPIO);
    
    // Enable global deep sleep hold
    gpio_deep_sleep_hold_en();
    ESP_LOGI(TAG, "Peripherals powered OFF and pad hold enabled");
}

uint8_t read_battery_percentage(void)
{
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_config, &adc1_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC oneshot unit: %s", esp_err_to_name(err));
        return 100;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    // BAT_IN is connected to GPIO1, which is ADC1 Channel 1 on ESP32-C3
    err = adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_1, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config ADC channel: %s", esp_err_to_name(err));
        adc_oneshot_del_unit(adc1_handle);
        return 100;
    }

    int raw_val = 0;
    int samples = 8;
    for (int i = 0; i < samples; i++) {
        int val = 0;
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_1, &val);
        raw_val += val;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    raw_val /= samples;
    adc_oneshot_del_unit(adc1_handle);

    // Convert raw reading to battery voltage
    // ESP32-C3 ADC range is 0 to 4095. Max voltage with 12dB attenuation is ~3.1V.
    // Battery divider is 100k/100k, so battery voltage = pin voltage * 2.
    float battery_voltage = ((float)raw_val / 4095.0f) * 3.1f * 2.0f;

    // Li-Po battery voltage ranges from 3.3V (0%) to 4.2V (100%)
    float min_volt = 3.3f;
    float max_volt = 4.2f;
    if (battery_voltage > max_volt) battery_voltage = max_volt;
    if (battery_voltage < min_volt) battery_voltage = min_volt;

    uint8_t pct = (uint8_t)(((battery_voltage - min_volt) / (max_volt - min_volt)) * 100.0f);
    if (pct > 100) pct = 100;
    ESP_LOGI(TAG, "Battery Raw ADC: %d, Voltage: %.2fV, Percentage: %d%%", raw_val, battery_voltage, pct);
    return pct;
}

uint8_t read_solar_voltage(void)
{
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_config, &adc1_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC oneshot unit: %s", esp_err_to_name(err));
        return 0;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    // SOL_IN is connected to GPIO0, which is ADC1 Channel 0 on ESP32-C3
    err = adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config ADC channel for solar: %s", esp_err_to_name(err));
        adc_oneshot_del_unit(adc1_handle);
        return 0;
    }

    int raw_val = 0;
    int samples = 8;
    for (int i = 0; i < samples; i++) {
        int val = 0;
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &val);
        raw_val += val;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    raw_val /= samples;
    adc_oneshot_del_unit(adc1_handle);

    // Convert raw reading to solar voltage
    // Max voltage with 12dB attenuation is ~3.1V.
    // Solar divider is 100k/100k, so Solar voltage = pin voltage * 2.
    float solar_voltage = ((float)raw_val / 4095.0f) * 3.1f * 2.0f;

    // A tenth of a volt resolution (e.g. 5.2V -> 52)
    uint8_t volt_x10 = (uint8_t)(solar_voltage * 10.0f);
    ESP_LOGI(TAG, "Solar Raw ADC: %d, Voltage: %.2fV, Decivolts: %d", raw_val, solar_voltage, volt_x10);
    return volt_x10;
}

static float get_single_reading(void)
{
    // Send a 10us HIGH pulse to TRIG
    gpio_set_level(TRIG_GPIO, 0);
    esp_rom_delay_us(2);
    gpio_set_level(TRIG_GPIO, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_GPIO, 0);

    // Wait for ECHO to go HIGH (timeout 30ms)
    int timeout = 30000;
    while (gpio_get_level(ECHO_GPIO) == 0 && timeout--) {
        esp_rom_delay_us(1);
    }
    if (timeout <= 0) {
        return -1.0f;
    }

    int64_t start_time = esp_timer_get_time();

    // Wait for ECHO to go LOW (timeout 30ms)
    timeout = 30000;
    while (gpio_get_level(ECHO_GPIO) == 1 && timeout--) {
        esp_rom_delay_us(1);
    }
    if (timeout <= 0) {
        return -1.0f;
    }

    int64_t end_time = esp_timer_get_time();
    int64_t duration = end_time - start_time;

    // Distance = duration / 2 * speed of sound (0.0343 cm/us)
    return (duration / 2.0f) * 0.0343f;
}

float read_filtered_ultrasonic_distance(void)
{
    float readings[3];
    int valid_count = 0;

    for (int i = 0; i < 3; i++) {
        float val = get_single_reading();
        if (val > 0.0f && val < 500.0f) {
            readings[valid_count++] = val;
        }
        vTaskDelay(pdMS_TO_TICKS(35));
    }

    if (valid_count == 0) {
        ESP_LOGW(TAG, "All ultrasonic readings failed!");
        return -1.0f;
    }

    // Sort valid readings (bubble sort)
    for (int i = 0; i < valid_count - 1; i++) {
        for (int j = 0; j < valid_count - i - 1; j++) {
            if (readings[j] > readings[j + 1]) {
                float temp = readings[j];
                readings[j] = readings[j + 1];
                readings[j + 1] = temp;
            }
        }
    }

    // Select the median
    float median = readings[valid_count / 2];
    ESP_LOGI(TAG, "Filtered Median Distance: %.2f cm (from %d valid samples)", median, valid_count);
    return median;
}

void ultrasonic_init(void)
{
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << TRIG_GPIO),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << ECHO_GPIO);
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE,
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE,
    gpio_config(&io_conf);
}

static void isolate_unused_gpios(void)
{
    // Note: I2C driver is completely uninitialized and not used anywhere in the Sender firmware.
    // The I2C hardware peripheral remains clock-gated (fully powered down) during active mode,
    // and is completely power-gated (0 uA) during deep sleep.
    
    // GPIO 8 is NC (Not Connected) on the schematic. Isolate to prevent leakage.
    gpio_reset_pin(GPIO_NUM_8);
    gpio_set_pull_mode(GPIO_NUM_8, GPIO_FLOATING);

    // GPIO 2 is NC (Not Connected) on the schematic. Isolate to prevent leakage.
    gpio_reset_pin(GPIO_NUM_2);
    gpio_set_pull_mode(GPIO_NUM_2, GPIO_FLOATING);
}

void deepsleep(uint32_t sleep_time_sec)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    int sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - sleep_enter_time.tv_usec) / 1000;

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        printf("Wake up from timer. Time spent in deep sleep: %dms\n", sleep_time_ms);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Enabling timer wakeup, %lus\n", (unsigned long)sleep_time_sec);
    esp_sleep_enable_timer_wakeup((uint64_t)sleep_time_sec * 1000000ULL);

    esp_set_deep_sleep_wake_stub(&wake_stub_example);

    ESP_LOGI(TAG, "Entering deep sleep");
    gettimeofday(&sleep_enter_time, NULL);

    isolate_unused_gpios();
    esp_deep_sleep_start();
}

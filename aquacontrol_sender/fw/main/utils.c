#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "utils.h"

#define TRIG_GPIO GPIO_NUM_19
#define ECHO_GPIO GPIO_NUM_18

float ultrasonic_data = 0.0f;
static const char *TAG = "Ultrasonic";

static void ultrasonic_task(void *pvParameters)
{
    while (1) {
        // Send a 10us HIGH pulse to TRIG
        gpio_set_level(TRIG_GPIO, 0);
        esp_rom_delay_us(2);
        gpio_set_level(TRIG_GPIO, 1);
        esp_rom_delay_us(10);
        gpio_set_level(TRIG_GPIO, 0);

        // Wait for ECHO to go HIGH and start timing
        int64_t start_time = 0, end_time = 0;

        // Wait for echo HIGH (timeout 50ms)
        int timeout = 50000;
        while (gpio_get_level(ECHO_GPIO) == 0 && timeout--) {
            esp_rom_delay_us(1);
        }
        if (timeout <= 0) {
            ESP_LOGW(TAG, "Echo start timeout");
            ultrasonic_data = -1;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        start_time = esp_timer_get_time();

        // Wait for echo LOW (timeout 50ms)
        timeout = 50000;
        while (gpio_get_level(ECHO_GPIO) == 1 && timeout--) {
            esp_rom_delay_us(1);
        }
        if (timeout <= 0) {
            ESP_LOGW(TAG, "Echo end timeout");
            ultrasonic_data = -1;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        end_time = esp_timer_get_time();
        int64_t duration = end_time - start_time;

        // Speed of sound = 34300 cm/s. Distance = time/2 * speed.
        ultrasonic_data = (duration / 2.0f) * 0.0343f;

        ESP_LOGI(TAG, "Measured distance: %.2f cm", ultrasonic_data);

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second delay
    }
}

void ultrasonic_init(void)
{
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << TRIG_GPIO),
    };
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << ECHO_GPIO);
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);

    xTaskCreate(ultrasonic_task, "ultrasonic_task", 2048, NULL, 5, NULL);
}

void deepsleep(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    int sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - sleep_enter_time.tv_usec) / 1000;

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        printf("Wake up from timer. Time spent in deep sleep: %dms\n", sleep_time_ms);
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    const int wakeup_time_sec = CONFIG_WAKE_UP_TIME;
    ESP_LOGI(TAG,"Enabling timer wakeup, %ds\n", wakeup_time_sec);
    esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000);

#if CONFIG_IDF_TARGET_ESP32
    // Isolate GPIO12 pin from external circuits. This is needed for modules
    // which have an external pull-up resistor on GPIO12 (such as ESP32-WROVER)
    // to minimize current consumption.
    rtc_gpio_isolate(GPIO_NUM_12);
#endif

    esp_set_deep_sleep_wake_stub(&wake_stub_example);

    ESP_LOGI(TAG,"Entering deep sleep");
    gettimeofday(&sleep_enter_time, NULL);

    esp_deep_sleep_start();
}

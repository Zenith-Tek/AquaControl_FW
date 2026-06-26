#ifndef __RC_GPIO_H__
#define __RC_GPIO_H__

#include <stdint.h>
#include "esp_timer.h"  // for esp_timer_get_time()
#include "sdkconfig.h"

// GPIO numbers mapped to Kconfig configurations
#define RELAY_GPIO CONFIG_RELAY_GPIO
#define SWITCH_GPIO CONFIG_SWITCH_GPIO

// Function to setup GPIOs (Relay output + Switch input with interrupt)
void setup_gpios(void);

#endif // __RC_GPIO_H__

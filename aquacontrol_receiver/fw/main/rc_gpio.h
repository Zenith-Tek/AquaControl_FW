#ifndef __RC_GPIO_H__
#define __RC_GPIO_H__

#include <stdint.h>
#include "esp_timer.h"  // for esp_timer_get_time()
// GPIO numbers
#define RELAY_GPIO 4
#define SWITCH_GPIO 27

// Function to setup GPIOs (Relay output + Switch input with interrupt)
void setup_gpios(void);

#endif // __RC_GPIO_H__

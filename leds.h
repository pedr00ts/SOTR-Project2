#ifndef LEDS_H
#define LEDS_H

#include <zephyr/drivers/gpio.h>

void leds_init(void);
void set_led_state(int led_index, uint8_t state);
void update_outputs(void);

#endif // LEDS_H

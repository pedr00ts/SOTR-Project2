#include "leds.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_control);

// Definir os LEDs (pinos GPIO)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1), gpios, {0});
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led2), gpios, {0});
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led3), gpios, {0});

// Inicializar os LEDs
void leds_init(void) {
    if (!device_is_ready(led0.port) || !device_is_ready(led1.port) ||
        !device_is_ready(led2.port) || !device_is_ready(led3.port)) {
        LOG_ERR("GPIO device is not ready for LEDs\n");
        return;
    }

    gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);  // Desliga os LEDs inicialmente
    gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led3, GPIO_OUTPUT_INACTIVE);

    LOG_INF("LEDs have been initialized successfully\n");
}

// Definir o estado de um LED (acender/desligar)
void set_led_state(int led_index, uint8_t state) {
    switch (led_index) {
        case 0:
            gpio_pin_set_dt(&led0, state);
            break;
        case 1:
            gpio_pin_set_dt(&led1, state);
            break;
        case 2:
            gpio_pin_set_dt(&led2, state);
            break;
        case 3:
            gpio_pin_set_dt(&led3, state);
            break;
        default:
            LOG_ERR("Invalid LED index: %d\n", led_index);
    }
}

// Atualizar os LEDs com base no RTDB
void update_outputs(void) {
    // Atualiza cada LED com o valor armazenado no RTDB
    for (int i = 0; i < 4; i++) {
        set_led_state(i, rtdb.outputs[i]);
    }
}

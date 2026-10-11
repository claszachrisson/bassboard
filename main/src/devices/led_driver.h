//
// Created by Clas Zachrisson on 2025-04-17.
//

#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include "lib_i2c.h"

enum led_state {
    LED_OFF = 0,
    LED_MAXPWM0 = 2,
    LED_MAXPWM1 = 3,
    LED_ON = 4,
    LED_ONESHOT = 5,
    LED_PWM0 = 6,
    LED_PWM1 = 7,
};

esp_err_t led_init();
void led_set_state(int p, led_state state);

#endif //LED_DRIVER_H

//
// Created by Clas Zachrisson on 2025-04-17.
//

#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include "i2c_device.h"

class LEDDriver : public I2CDevice{
public:
    LEDDriver();
    enum LED_state {
        LED_OFF = 0,
        LED_MAXPWM0 = 2,
        LED_MAXPWM1 = 3,
        LED_ON = 4,
        LED_ONESHOT = 5,
        LED_PWM0 = 6,
        LED_PWM1 = 7,
    };

    void set_state(int p, LED_state state);
private:
    uint8_t select0 = 0xFF;
    uint8_t select1 = 0xFF;
    uint8_t select2 = 0xFF;

    enum control_port_address
    {
        SELECT0                 = 0x00,
        SELECT1                 = 0x01,
        SELECT2                 = 0x02,
        FADE_ON_TIME            = 0x03,
        FULLY_ON_TIME           = 0x04,
        FADE_OFF_TIME           = 0x05,
        FIRST_FULLY_OFF_TIME    = 0x06,
        SECOND_FULLY_OFF_TIME   = 0x07,
        MAXIMUM_INTENSITY       = 0x08,
        MASTER_INTENSITY        = 0x09,
        INITIALIZATION          = 0x0A,
    };
    enum time_code
    {
        T_0_MS = 0x00,
        T_64_MS = 0x01,
        T_128_MS = 0x02,
        T_192_MS = 0x03,
        T_256_MS = 0x04,
        T_384_MS = 0x05,
        T_512_MS = 0x06,
        T_768_MS = 0x07,
        T_1024_MS = 0x08,
        T_1536_MS = 0x09,
        T_2048_MS = 0x0A,
        T_3072_MS = 0x0B,
        T_4096_MS = 0x0C,
        T_5760_MS = 0x0D,
        T_8128_MS = 0x0E,
        T_16320_MS = 0x0F,
    };

    void read_select();
    void write_select();

};

#endif //LED_DRIVER_H
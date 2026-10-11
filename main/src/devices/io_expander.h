//
// Created by Clas Zachrisson on 2025-04-17.
//

#ifndef IO_EXPANDER_H
#define IO_EXPANDER_H

#include "lib_i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"


enum external_gpio
{
    EXIO_NUM_00 = 0x00,
    EXIO_NUM_01 = 0x01,
    EXIO_NUM_02 = 0x02,
    EXIO_NUM_03 = 0x03,
    EXIO_NUM_04 = 0x04,
    EXIO_NUM_05 = 0x05,
    EXIO_NUM_06 = 0x06,
    EXIO_NUM_07 = 0x07,
    EXIO_NUM_10 = 0x10,
    EXIO_NUM_11 = 0x11,
    EXIO_NUM_12 = 0x12,
    EXIO_NUM_13 = 0x13,
    EXIO_NUM_14 = 0x14,
    EXIO_NUM_15 = 0x15,
    EXIO_NUM_16 = 0x16,
    EXIO_NUM_17 = 0x17,
};

static constexpr int IOEX_OUTPUT = 0;
static constexpr int IOEX_INPUT = 1;

enum ioex_switch_event_type
{
    IOEX_SWITCH_SHORT,
    IOEX_SWITCH_LONG,
    IOEX_SWITCH_DOUBLE,
};

typedef void (*ioex_switch_event_cb_t)(int port, int bit, ioex_switch_event_type type, uint32_t press_ms, void* user_ctx);

esp_err_t ioex_init();
void ioex_set_direction(uint8_t exio_num, int dir);
void ioex_set_level(uint8_t exio_num, int level);
void ioex_register_switch(uint8_t exio_num, bool active_low = true);
void ioex_register_switch_event_cb(ioex_switch_event_cb_t cb, void* user_ctx = nullptr);

#endif //IO_EXPANDER_H

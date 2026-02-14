//
// Created by Clas Zachrisson on 2025-04-17.
//

#ifndef IO_EXPANDER_H
#define IO_EXPANDER_H

#include "i2c_device.h"
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

class IOExpander : public I2CDevice {
public:
    static int OUTPUT;
    static int INPUT;

    IOExpander();
    void set_direction(uint8_t exio_num, int dir);
    void set_level(uint8_t exio_num, int level);
private:
    static QueueHandle_t intr_evt_queue;
    static TimerHandle_t debounce_timer;

    uint8_t io_dirs[2] = {0xFF, 0xFF}; // Inputs by default
    uint8_t output_levels[2] = {0xFF, 0xFF}; // HIGH by default
    enum control_port : uint8_t {
        INPUT_PORT0 = 0x00,
        INPUT_PORT1 = 0x01,
        OUTPUT_PORT0 = 0x02,
        OUTPUT_PORT1 = 0x03,
        POL_INV_PORT0 = 0x04,
        POL_INV_PORT1 = 0x05,
        DIR_PORT0 = 0x06,
        DIR_PORT1 = 0x07,
    };

    static void IRAM_ATTR isr_handler(void* arg);
    static void isr_task(void* arg);
    static void debounce_timer_cb(TimerHandle_t timer);

    esp_err_t write_directions();
    esp_err_t write_levels();
};

#endif //IO_EXPANDER_H

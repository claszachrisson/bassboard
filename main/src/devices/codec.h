//
// Created by Clas Zachrisson on 2025-04-18.
//

#ifndef CODEC_H
#define CODEC_H

#include "types.h"
#include "lib_i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "io_expander.h"

class Codec : public I2CDevice {
public:
    Codec();

    void register_event_cb(BBEventCallback);
    void read(int16_t* buffer, int frames);
    void read(float* buffer, int frames);

    static int get_frame_size();
    static int16_t* get_input_buffer();
    static int16_t* get_output_buffer();
    // static void mute_output();
    // static void unmute_output();
    static void set_dsp_task_handle(TaskHandle_t handle);
    static void set_output_frame_ready(int frame_start);
    static volatile int output_frame;

private:
    static i2s_chan_handle_t tx_handle;
    static i2s_chan_handle_t rx_handle;
    static TaskHandle_t dsp_task_handle;
    uint32_t sample_rate;
    // static bool transmitting;

    static IRAM_ATTR bool i2s_rx_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);
    static IRAM_ATTR bool i2s_tx_sent_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);
    // static size_t streamBufferReceive(void* buffer, size_t size, TickType_t ticks_to_wait);
};

#endif //CODEC_H

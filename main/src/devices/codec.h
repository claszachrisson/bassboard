//
// Created by Clas Zachrisson on 2025-04-18.
//

#ifndef CODEC_H
#define CODEC_H

#include "types.h"
#include "i2c_device.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "io_expander.h"

class Codec : public I2CDevice {
public:
    explicit Codec(IOExpander* io_expander);

    void register_event_cb(BBEventCallback);
    void read(int16_t* buffer, int frames);
    void read(float* buffer, int frames);

    static int get_frame_size();
    static void mute_output();
    static void unmute_output();
private:
    static i2s_chan_handle_t tx_handle;
    static i2s_chan_handle_t rx_handle;
    static TaskHandle_t stream_task_handle;
    IOExpander* io_expander;
    uint32_t sample_rate;
    static bool transmitting;

    static IRAM_ATTR bool i2s_rx_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);
    static IRAM_ATTR bool i2s_tx_sent_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);
    static size_t streamBufferReceive(void* buffer, size_t size, TickType_t ticks_to_wait);
};

#endif //CODEC_H

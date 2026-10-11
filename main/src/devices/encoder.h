//
// Created by Codex on 2026-03-02.
//

#ifndef ENCODER_H
#define ENCODER_H

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef void (*encoder_step_cb_t)(int direction, int32_t position, void* user_ctx);

class Encoder {
public:
    static constexpr gpio_num_t PIN_A = GPIO_NUM_36;
    static constexpr gpio_num_t PIN_B = GPIO_NUM_39;

    Encoder();
    bool read_a() const;
    bool read_b() const;
    uint8_t read_state() const;
    int32_t get_position() const;
    void reset_position();
    void register_step_cb(encoder_step_cb_t cb, void* user_ctx = nullptr);

private:
    static QueueHandle_t intr_evt_queue;
    static void IRAM_ATTR isr_handler(void* arg);
    static void isr_task(void* arg);
    void process_transition(uint8_t new_state);

    int32_t position = 0;
    uint8_t prev_state = 0;
    int8_t transition_accumulator = 0;
    encoder_step_cb_t step_cb = nullptr;
    void* step_user_ctx = nullptr;
};

#endif //ENCODER_H

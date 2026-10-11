//
// Created by Codex on 2026-03-02.
//

#include "encoder.h"

#include <cstdint>

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/task.h"

static constexpr char TAG[] = "ENC";
QueueHandle_t Encoder::intr_evt_queue = nullptr;

Encoder::Encoder()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_A) | (1ULL << PIN_B);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    intr_evt_queue = xQueueCreate(16, sizeof(uint32_t));
    xTaskCreate(isr_task, "enc_isr_task", 4096, this, 1, nullptr);

    auto add_isr_handler = [](const gpio_num_t pin, void* arg) {
        esp_err_t err = gpio_isr_handler_add(pin, isr_handler, arg);
        if (err != ESP_OK)
        {
            const esp_err_t install_err = gpio_install_isr_service(0);
            if (install_err != ESP_OK && install_err != ESP_ERR_INVALID_STATE)
            {
                ESP_ERROR_CHECK(install_err);
            }
            err = gpio_isr_handler_add(pin, isr_handler, arg);
        }
        ESP_ERROR_CHECK(err);
    };

    add_isr_handler(PIN_A, reinterpret_cast<void*>(static_cast<uintptr_t>(PIN_A)));
    add_isr_handler(PIN_B, reinterpret_cast<void*>(static_cast<uintptr_t>(PIN_B)));

    prev_state = read_state();
    ESP_LOGI(
        TAG,
        "Encoder initialized. A=%d B=%d state=%u",
        static_cast<int>(PIN_A),
        static_cast<int>(PIN_B),
        static_cast<unsigned>(prev_state)
    );
}

bool Encoder::read_a() const
{
    return gpio_get_level(PIN_A) != 0;
}

bool Encoder::read_b() const
{
    return gpio_get_level(PIN_B) != 0;
}

uint8_t Encoder::read_state() const
{
    return static_cast<uint8_t>((read_a() << 1) | read_b());
}

int32_t Encoder::get_position() const
{
    return position;
}

void Encoder::reset_position()
{
    position = 0;
    transition_accumulator = 0;
}

void Encoder::register_step_cb(const encoder_step_cb_t cb, void* user_ctx)
{
    step_cb = cb;
    step_user_ctx = user_ctx;
}

void IRAM_ATTR Encoder::isr_handler(void* arg)
{
    const uint32_t gpio_num = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg));
    xQueueSendFromISR(intr_evt_queue, &gpio_num, nullptr);
}

void Encoder::isr_task(void* arg)
{
    auto* self = static_cast<Encoder*>(arg);
    uint32_t gpio_num = 0;
    while (true)
    {
        if (xQueueReceive(intr_evt_queue, &gpio_num, portMAX_DELAY))
        {
            (void) gpio_num;
            self->process_transition(self->read_state());
        }
    }
}

void Encoder::process_transition(const uint8_t new_state)
{
    static constexpr int8_t transition_lut[16] = {
        0,  1, -1,  0,
       -1,  0,  0,  1,
        1,  0,  0, -1,
        0, -1,  1,  0
    };

    const uint8_t transition_index = static_cast<uint8_t>((prev_state << 2) | new_state);
    prev_state = new_state;

    // Invert decoded direction to match the board's physical encoder orientation.
    const int8_t delta = static_cast<int8_t>(-transition_lut[transition_index]);
    if (delta == 0)
    {
        return;
    }

    if ((transition_accumulator > 0 && delta < 0) || (transition_accumulator < 0 && delta > 0))
    {
        transition_accumulator = 0;
    }

    transition_accumulator = static_cast<int8_t>(transition_accumulator + delta);
    if (transition_accumulator >= 4)
    {
        transition_accumulator = 0;
        position++;
        ESP_LOGI(TAG, "CW step. pos=%ld", static_cast<long>(position));
        if (step_cb)
        {
            step_cb(+1, position, step_user_ctx);
        }
    }
    else if (transition_accumulator <= -4)
    {
        transition_accumulator = 0;
        position--;
        ESP_LOGI(TAG, "CCW step. pos=%ld", static_cast<long>(position));
        if (step_cb)
        {
            step_cb(-1, position, step_user_ctx);
        }
    }
}

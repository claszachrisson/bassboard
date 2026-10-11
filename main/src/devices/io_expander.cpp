//
// Created by Clas Zachrisson on 2025-04-17.
//
#include "io_expander.h"

static constexpr char TAG[] = "IOEX";
static constexpr gpio_num_t INTERRUPT_PIN = GPIO_NUM_34;
static constexpr gpio_int_type_t INT_TYPE = GPIO_INTR_NEGEDGE;
static constexpr uint32_t SHORT_PRESS_MAX_MS = 500;
static constexpr uint32_t DOUBLE_PRESS_MAX_GAP_MS = 300;

static constexpr uint8_t INPUT_PORT0 = 0x00;
static constexpr uint8_t OUTPUT_PORT0 = 0x02;
static constexpr uint8_t DIR_PORT0 = 0x06;

struct IOExState {
    I2CDevice* device = nullptr;
    QueueHandle_t intr_evt_queue = nullptr;
    uint8_t io_dirs[2] = {0xFF, 0xFF};
    uint8_t output_levels[2] = {0xFF, 0xFF};
    uint8_t port[2] = {};
    uint8_t switch_masks[2] = {};
    uint8_t switch_active_low_masks[2] = {};
    uint8_t switch_pressed_masks[2] = {};
    uint8_t switch_pending_short_masks[2] = {};
    TickType_t switch_press_ticks[2][8] = {};
    TickType_t switch_pending_short_ticks[2][8] = {};
    uint32_t switch_pending_short_press_ms[2][8] = {};
    ioex_switch_event_cb_t switch_event_cb = nullptr;
    void* switch_event_user_ctx = nullptr;
    bool initialized = false;
};

static IOExState g_ioex = {};

static esp_err_t ioex_write_directions()
{
    return g_ioex.device->register_write(DIR_PORT0, g_ioex.io_dirs, 2);
}

static esp_err_t ioex_write_levels()
{
    return g_ioex.device->register_write(OUTPUT_PORT0, g_ioex.output_levels, 2);
}

static void ioex_flush_pending_short_presses(const TickType_t now_ticks)
{
    for (int port_idx = 0; port_idx < 2; port_idx++)
    {
        for (int bit = 0; bit < 8; bit++)
        {
            const uint8_t bit_mask = static_cast<uint8_t>(1U << bit);
            if ((g_ioex.switch_pending_short_masks[port_idx] & bit_mask) == 0)
            {
                continue;
            }

            const TickType_t elapsed_ticks = now_ticks - g_ioex.switch_pending_short_ticks[port_idx][bit];
            const uint32_t elapsed_ms = static_cast<uint32_t>(pdTICKS_TO_MS(elapsed_ticks));
            if (elapsed_ms <= DOUBLE_PRESS_MAX_GAP_MS)
            {
                continue;
            }

            g_ioex.switch_pending_short_masks[port_idx] &= static_cast<uint8_t>(~bit_mask);
            const uint32_t press_ms = g_ioex.switch_pending_short_press_ms[port_idx][bit];
            if (g_ioex.switch_event_cb)
            {
                g_ioex.switch_event_cb(port_idx, bit, IOEX_SWITCH_SHORT, press_ms, g_ioex.switch_event_user_ctx);
            }
            else
            {
                ESP_LOGI(TAG, "Switch P%d.%d RELEASE SHORT", port_idx, bit);
            }
        }
    }
}

static void IRAM_ATTR ioex_isr_handler(void* arg)
{
    (void) arg;
    xQueueSendFromISR(g_ioex.intr_evt_queue, nullptr, nullptr);
}

static void ioex_isr_task(void* arg)
{
    (void) arg;
    uint8_t temp_port[2] = {};
    while (true)
    {
        if (xQueueReceive(g_ioex.intr_evt_queue, nullptr, pdMS_TO_TICKS(20)))
        {
            g_ioex.device->register_read(INPUT_PORT0, temp_port, 2);
            for (int i = 0; i < 2; i++)
            {
                if (g_ioex.port[i] != temp_port[i])
                {
                    const uint8_t old_port = g_ioex.port[i];
                    g_ioex.port[i] = temp_port[i];

                    const uint8_t changed_bits = old_port ^ g_ioex.port[i];
                    const uint8_t changed_switches = changed_bits & g_ioex.switch_masks[i];
                    for (int bit = 0; bit < 8; bit++)
                    {
                        const uint8_t bit_mask = static_cast<uint8_t>(1U << bit);
                        if ((changed_switches & bit_mask) == 0)
                        {
                            continue;
                        }

                        const bool new_level = (g_ioex.port[i] & bit_mask) != 0;
                        const bool active_low = (g_ioex.switch_active_low_masks[i] & bit_mask) != 0;
                        const bool is_press = active_low ? !new_level : new_level;
                        const TickType_t now = xTaskGetTickCount();

                        if (is_press)
                        {
                            g_ioex.switch_pressed_masks[i] |= bit_mask;
                            g_ioex.switch_press_ticks[i][bit] = now;
                            continue;
                        }

                        if ((g_ioex.switch_pressed_masks[i] & bit_mask) == 0)
                        {
                            continue;
                        }

                        g_ioex.switch_pressed_masks[i] &= static_cast<uint8_t>(~bit_mask);
                        const TickType_t press_ticks = now - g_ioex.switch_press_ticks[i][bit];
                        const uint32_t press_ms = static_cast<uint32_t>(pdTICKS_TO_MS(press_ticks));
                        if (press_ms >= SHORT_PRESS_MAX_MS)
                        {
                            g_ioex.switch_pending_short_masks[i] &= static_cast<uint8_t>(~bit_mask);
                            if (g_ioex.switch_event_cb)
                            {
                                g_ioex.switch_event_cb(i, bit, IOEX_SWITCH_LONG, press_ms, g_ioex.switch_event_user_ctx);
                            }
                            else
                            {
                                ESP_LOGI(TAG, "Switch P%d.%d RELEASE LONG (%lu ms)", i, bit, static_cast<unsigned long>(press_ms));
                            }
                            continue;
                        }

                        if ((g_ioex.switch_pending_short_masks[i] & bit_mask) != 0)
                        {
                            const TickType_t gap_ticks = now - g_ioex.switch_pending_short_ticks[i][bit];
                            const uint32_t gap_ms = static_cast<uint32_t>(pdTICKS_TO_MS(gap_ticks));
                            if (gap_ms <= DOUBLE_PRESS_MAX_GAP_MS)
                            {
                                g_ioex.switch_pending_short_masks[i] &= static_cast<uint8_t>(~bit_mask);
                                if (g_ioex.switch_event_cb)
                                {
                                    g_ioex.switch_event_cb(i, bit, IOEX_SWITCH_DOUBLE, press_ms, g_ioex.switch_event_user_ctx);
                                }
                                else
                                {
                                    ESP_LOGI(
                                        TAG,
                                        "Switch P%d.%d RELEASE DOUBLE (%lu ms, gap=%lu ms)",
                                        i,
                                        bit,
                                        static_cast<unsigned long>(press_ms),
                                        static_cast<unsigned long>(gap_ms)
                                    );
                                }
                                continue;
                            }
                        }

                        g_ioex.switch_pending_short_masks[i] |= bit_mask;
                        g_ioex.switch_pending_short_ticks[i][bit] = now;
                        g_ioex.switch_pending_short_press_ms[i][bit] = press_ms;
                    }
                }
            }
        }

        ioex_flush_pending_short_presses(xTaskGetTickCount());
    }
}

esp_err_t ioex_init()
{
    if (g_ioex.initialized)
    {
        return ESP_OK;
    }

    static I2CDevice ioex_device(0x20, TAG);
    g_ioex.device = &ioex_device;

    gpio_config_t io_conf = {};
    io_conf.intr_type = INT_TYPE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << INTERRUPT_PIN;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    g_ioex.intr_evt_queue = xQueueCreate(10, 0);
    xTaskCreate(ioex_isr_task, "ioex_isr_task", 4096, nullptr, 1, nullptr);

    esp_err_t isr_add_err = gpio_isr_handler_add(INTERRUPT_PIN, ioex_isr_handler, nullptr);
    if (isr_add_err != ESP_OK)
    {
        const esp_err_t isr_install_err = gpio_install_isr_service(0);
        if (isr_install_err != ESP_OK && isr_install_err != ESP_ERR_INVALID_STATE)
        {
            ESP_ERROR_CHECK(isr_install_err);
        }
        isr_add_err = gpio_isr_handler_add(INTERRUPT_PIN, ioex_isr_handler, nullptr);
    }
    ESP_ERROR_CHECK(isr_add_err);

    g_ioex.device->register_read(INPUT_PORT0, &g_ioex.port[0], 1);
    ESP_LOGI(TAG, "Input port 0: 0x%02X", g_ioex.port[0]);
    g_ioex.device->register_read(INPUT_PORT0 + 1, &g_ioex.port[1], 1);
    ESP_LOGI(TAG, "Input port 1: 0x%02X", g_ioex.port[1]);

    g_ioex.initialized = true;
    return ESP_OK;
}

void ioex_set_direction(uint8_t exio_num, const int dir)
{
    if (!g_ioex.initialized)
    {
        return;
    }

    uint8_t* port = &g_ioex.io_dirs[0];
    if (exio_num & 0x10)
    {
        port++;
        exio_num = exio_num & 0x0F;
    }
    *port = (*port & ~(1 << exio_num)) | ((dir & 1) << exio_num);
    ioex_write_directions();
}

void ioex_set_level(uint8_t exio_num, const int level)
{
    if (!g_ioex.initialized)
    {
        return;
    }

    uint8_t* port = &g_ioex.output_levels[0];
    if (exio_num & 0x10)
    {
        port++;
        exio_num = exio_num & 0x0F;
    }
    *port = (*port & ~(1 << exio_num)) | ((level & 1) << exio_num);
    ioex_write_levels();
}

void ioex_register_switch(uint8_t exio_num, const bool active_low)
{
    if (!g_ioex.initialized)
    {
        return;
    }

    uint8_t* switch_mask_port = &g_ioex.switch_masks[0];
    uint8_t* switch_active_low_mask_port = &g_ioex.switch_active_low_masks[0];
    if (exio_num & 0x10)
    {
        switch_mask_port++;
        switch_active_low_mask_port++;
        exio_num = exio_num & 0x0F;
    }

    const uint8_t bit_mask = static_cast<uint8_t>(1U << exio_num);
    *switch_mask_port |= bit_mask;
    if (active_low)
    {
        *switch_active_low_mask_port |= bit_mask;
    }
    else
    {
        *switch_active_low_mask_port &= static_cast<uint8_t>(~bit_mask);
    }
}

void ioex_register_switch_event_cb(const ioex_switch_event_cb_t cb, void* user_ctx)
{
    g_ioex.switch_event_cb = cb;
    g_ioex.switch_event_user_ctx = user_ctx;
}

//
// Created by Clas Zachrisson on 2025-04-17.
//
#include "led_driver.h"

static constexpr uint8_t AUTO_INCREMENT = 0x00;
static constexpr uint8_t SELECT0 = 0x00;
static constexpr uint8_t SELECT1 = 0x01;
static constexpr uint8_t SELECT2 = 0x02;
static constexpr uint8_t FADE_ON_TIME = 0x03;
static constexpr uint8_t FADE_OFF_TIME = 0x05;
static constexpr uint8_t FIRST_FULLY_OFF_TIME = 0x06;
static constexpr uint8_t SECOND_FULLY_OFF_TIME = 0x07;
static constexpr uint8_t MAXIMUM_INTENSITY = 0x08;
static constexpr uint8_t MASTER_INTENSITY = 0x09;
static constexpr uint8_t INITIALIZATION = 0x0A;

static constexpr uint8_t T_0_MS = 0x00;
static constexpr uint8_t T_384_MS = 0x05;
static constexpr uint8_t T_512_MS = 0x06;
static constexpr uint8_t T_768_MS = 0x07;

struct LedState {
    I2CDevice* device = nullptr;
    uint8_t select0 = 0xFF;
    uint8_t select1 = 0xFF;
    uint8_t select2 = 0xFF;
    bool initialized = false;
};

static LedState g_led = {};

static void led_write_select()
{
    g_led.device->register_write(SELECT0 | AUTO_INCREMENT, g_led.select0);
    g_led.device->register_write(SELECT1 | AUTO_INCREMENT, g_led.select1);
    g_led.device->register_write(SELECT2 | AUTO_INCREMENT, g_led.select2);
}

esp_err_t led_init()
{
    if (g_led.initialized)
    {
        return ESP_OK;
    }

    static I2CDevice led_device(0x45, "LED");
    g_led.device = &led_device;

    g_led.device->register_write(MAXIMUM_INTENSITY, 0xFF);
    g_led.device->register_write(MASTER_INTENSITY, 0x02);
    g_led.device->register_write(FADE_ON_TIME, T_512_MS | (T_384_MS << 4));
    g_led.device->register_write(FADE_OFF_TIME, T_768_MS | (T_512_MS << 4));
    g_led.device->register_write(FIRST_FULLY_OFF_TIME, T_0_MS);
    g_led.device->register_write(SECOND_FULLY_OFF_TIME, T_0_MS);
    g_led.device->register_write(INITIALIZATION, 0xFF);
    g_led.device->register_write(SELECT0, 0xFF);
    g_led.device->register_write(SELECT1, 0xFF);
    g_led.device->register_write(SELECT2, 0xFF);

    g_led.initialized = true;
    return ESP_OK;
}

void led_set_state(const int p, const led_state state)
{
    if (!g_led.initialized)
    {
        return;
    }

    g_led.select0 = (g_led.select0 & ~(1 << p)) | ((state & 1) << p);
    g_led.select1 = (g_led.select1 & ~(1 << p)) | ((state & 2) / 2 << p);
    g_led.select2 = (g_led.select2 & ~(1 << p)) | ((state & 4) / 4 << p);
    led_write_select();
}

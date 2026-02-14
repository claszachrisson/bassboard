//
// Created by Clas Zachrisson on 2025-04-21.
//
#include "lcd.h"
#include <array>
#include <cstring>
#include "driver/gpio.h"

//#include "esp_attr.h"

constexpr ledc_mode_t LEDC_MODE = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_bit_t LEDC_DUTY_RES = LEDC_TIMER_8_BIT;
constexpr ledc_timer_t LEDC_TIMER = LEDC_TIMER_0;
constexpr uint32_t LEDC_FREQUENCY = 100;
constexpr gpio_num_t LEDC_OUTPUT_IO = GPIO_NUM_4;
constexpr uint32_t LEDC_DUTY = 32; // Set duty to 50%. (2 ** 8) * 50% = 128
constexpr ledc_channel_t LEDC_CHANNEL = LEDC_CHANNEL_0;

LCD::LCD(IOExpander* io_expander)
    : io_expander(io_expander)
{
    gpio_reset_pin(GPIO_NUM_15);
    gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_21, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_22, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_5,0);
    gpio_set_level(GPIO_NUM_15,0);
    gpio_set_level(GPIO_NUM_18,0);
    gpio_set_level(GPIO_NUM_19,0);
    gpio_set_level(GPIO_NUM_21,0);
    gpio_set_level(GPIO_NUM_22,0);
    io_expander->set_direction(EXIO_NUM_17, IOExpander::OUTPUT);
    io_expander->set_level(EXIO_NUM_17, 0);

    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .gpio_num       = LEDC_OUTPUT_IO,
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

    io_expander->set_level(EXIO_NUM_17, 1);

    ESP_LOGI("LCD", "LCD configured successfully");

}
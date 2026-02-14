//
// Created by Clas Zachrisson on 2025-04-17.
//
#include "io_expander.h"

gpio_num_t INTERRUPT_PIN = GPIO_NUM_34;
gpio_int_type_t INT_TYPE = GPIO_INTR_NEGEDGE;
int INT_COOLDOWN = 200; // milliseconds

QueueHandle_t IOExpander::intr_evt_queue = nullptr;
portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

TimerHandle_t IOExpander::debounce_timer = xTimerCreate(
    "debounceTimer",
    pdMS_TO_TICKS(INT_COOLDOWN),  // debounce period
    pdFALSE,             // one-shot timer
    nullptr,
    debounce_timer_cb
);
int IOExpander::OUTPUT = 0;
int IOExpander::INPUT = 1;

IOExpander::IOExpander()
    : I2CDevice(0x20, "IOExpander")
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = INT_TYPE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << INTERRUPT_PIN;
    gpio_config(&io_conf);

    intr_evt_queue = xQueueCreate(10, 0);
    xTaskCreate(isr_task, "ioexp_isr_task", 2048, this, 5, nullptr);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(INTERRUPT_PIN, isr_handler, nullptr);

    uint8_t data;
    this->register_read(0x00, &data, 1);
    ESP_LOGI(this->device_name, "Input port 0: 0x%02X", data);
    this->register_read(0x01, &data, 1);
    ESP_LOGI(this->device_name, "Input port 1: 0x%02X", data);
}

void IRAM_ATTR IOExpander::isr_handler(void* arg)
{
    portENTER_CRITICAL_ISR(&spinlock);
    gpio_set_intr_type(INTERRUPT_PIN, GPIO_INTR_DISABLE);
    portEXIT_CRITICAL_ISR(&spinlock);

    xQueueSendFromISR(intr_evt_queue, nullptr, nullptr);

    xTimerStartFromISR(debounce_timer, nullptr);
}

void IOExpander::isr_task(void* arg)
{
    auto* self = static_cast<IOExpander*>(arg);
    uint8_t ports[2] = {};
    while (true)
    {
        if (xQueueReceive(intr_evt_queue, nullptr, portMAX_DELAY))
        {
            self->register_read(0x00, &ports[0], std::size(ports));
            ESP_LOGI("IOExpander", "Interrupt. P0=0x%02X, P1=0x%02X", ports[0], ports[1]);
        }
    }
}

void IOExpander::debounce_timer_cb(TimerHandle_t timer)
{
    gpio_set_intr_type(INTERRUPT_PIN, INT_TYPE);
}

void IOExpander::set_direction(uint8_t exio_num, const int dir)
{
    uint8_t* port = &this->io_dirs[0];
    if (exio_num & 0x10)
    {
        port++;
        exio_num = exio_num & 0x0F;
    }
    *port = (*port & ~(1 << exio_num)) | ((dir & 1) << exio_num); // Set position exio_num in port to dir

    this->write_directions();
}

void IOExpander::set_level(uint8_t exio_num, const int level)
{
    uint8_t* port = &this->output_levels[0];
    if (exio_num & 0x10)
    {
        port++;
        exio_num = exio_num & 0x0F;
    }
    *port = (*port & ~(1 << exio_num)) | ((level & 1) << exio_num); // Set position exio_num in port to dir

    this->write_levels();
}

esp_err_t IOExpander::write_directions()
{
    return this->register_write(DIR_PORT0, this->io_dirs, 2);
}
esp_err_t IOExpander::write_levels()
{
    return this->register_write(OUTPUT_PORT0, this->output_levels, 2);
}

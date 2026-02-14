//
// Created by Clas Zachrisson on 2025-04-17.
//
#include "led_driver.h"

constexpr uint8_t AUTO_INCREMENT = 0x00;

LEDDriver::LEDDriver()
    : I2CDevice(0x45, "LED")
{
    this->register_write(MAXIMUM_INTENSITY, 0xFF);
    this->register_write(MASTER_INTENSITY, 0x0F);
    this->register_write(FADE_ON_TIME, T_512_MS | (T_384_MS << 4));
    this->register_write(FADE_OFF_TIME, T_768_MS | (T_512_MS << 4));
    this->register_write(FIRST_FULLY_OFF_TIME, T_0_MS);
    this->register_write(SECOND_FULLY_OFF_TIME, T_0_MS);
    this->register_write(INITIALIZATION, 0xFF);
    this->register_write(SELECT0, 0xFF);
    this->register_write(SELECT1, 0xFF);
    this->register_write(SELECT2, 0xFF);
}

void LEDDriver::set_state(int p, LED_state state){
    ESP_LOGI(this->device_name, "State before: Select0: 0x%02X\tSelect1: 0x%02X\tSelect2: 0x%02X", select0, select1, select2);
    select0 = (select0 & ~(1 << p)) | ((state & 1) << p);
    select1 = (select1 & ~(1 << p)) | ((state & 2) << (p-1));
    select2 = (select2 & ~(1 << p)) | ((state & 4) << (p-2));
    ESP_LOGI(this->device_name, "Select0: 0x%02X\tSelect1: 0x%02X\tSelect2: 0x%02X", select0, select1, select2);
    this->write_select();
}

void LEDDriver::write_select()
{
    this->register_write(SELECT0 | AUTO_INCREMENT, select0);
    this->register_write(SELECT1 | AUTO_INCREMENT, select1);
    this->register_write(SELECT2 | AUTO_INCREMENT, select2);
}
void LEDDriver::read_select(){}


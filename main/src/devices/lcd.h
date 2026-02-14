//
// Created by Clas Zachrisson on 2025-04-21.
//

#ifndef LCD_H
#define LCD_H

//#include "i2c_device.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "io_expander.h"
#include "driver/ledc.h"

class LCD {//: public I2CDevice {
public:
    LCD(IOExpander* io_expander);
private:
    IOExpander* io_expander;

};

#endif //LCD_H

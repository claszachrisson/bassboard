//
// Created by Clas Zachrisson on 2025-06-14.
//

#include "eeprom.h"

EEPROM::EEPROM()
    : I2CDevice(0x50, "EEPROM")
{}

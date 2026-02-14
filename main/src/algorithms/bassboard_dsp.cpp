//
// Created by Clas Zachrisson on 2025-04-18.
//
#include "bassboard_dsp.h"

#include <esp_log.h>
#include <sys/types.h>
#include <cmath>
#include <array>
#include <type_traits>
#include <cstdlib>
//#include <vector>
#include <algorithm>
#include <cstdint>

BBDSP::BBDSP(int sample_rate)
    : sample_rate(sample_rate)
{

}
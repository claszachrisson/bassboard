//
// Created by Clas Zachrisson on 2025-12-20.
//

#ifndef PITCHYIN_H
#define PITCHYIN_H

#include <esp_log.h>
#include <sys/types.h>
#include <cmath>
#include <array>
#include <type_traits>
#include <cstdlib>
//#include <vector>
#include <algorithm>
//#include <cstdint>

constexpr char YIN_TAG[] = "Yin";

class Yin{
public:
    Yin();
    Yin(float sampleRate, int bufferSize);
    void configure(float yinSampleRate,int yinBufferSize);

    float getPitch(float* buffer);
    float getProbability();

private:

    float threshold;
    // int bufferSize;
    // int halfBufferSize;
    float sampleRate;
    float* yin;
    int yinSize;
    float probability;
    int minTau, maxTau;
    int harmonics;
};



#endif //PITCHYIN_H

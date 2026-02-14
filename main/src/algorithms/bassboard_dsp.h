//
// Created by Clas Zachrisson on 2025-04-18.
//

#ifndef BASSBOARD_DSP_H
#define BASSBOARD_DSP_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


class BBDSP {
public:
    explicit BBDSP(int sample_rate);

private:
    int sample_rate;

    //void enable_FFT();
};


#endif //BASSBOARD_DSP_H

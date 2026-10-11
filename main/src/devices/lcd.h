#pragma once
#include "dsp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <vector>

// Firmware-lifetime device. Only task_main accesses LVGL or the panel.
class LCD {
public:
    explicit LCD(int capacity);
    LCD(const LCD&) = delete;
    LCD& operator=(const LCD&) = delete;
    static void block_changed(int slot, const DSPBlockSnapshot& snapshot, void* ctx);
    void set_brightness(uint8_t brightness);
    void request_hardware_reset();
private:
    static void task_main(void* ctx);
    void run();
    SemaphoreHandle_t model_mutex_ = nullptr;
    TaskHandle_t task_ = nullptr;
    std::vector<DSPBlockSnapshot> model_;
    bool dirty_ = true;
    bool reset_requested_ = false;
    uint8_t brightness_ = 128;
};

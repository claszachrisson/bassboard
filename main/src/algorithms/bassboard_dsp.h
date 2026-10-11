//
// Created by Clas Zachrisson on 2025-04-18.
//

#ifndef BASSBOARD_DSP_H
#define BASSBOARD_DSP_H

#include <codec.h>
#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "dsp_types.h"

#include "esp_heap_caps.h"

class DSPBlock
{
public:
    // ESP-IDF dispatches atomic operations differently for PSRAM. Keep block
    // control values in internal RAM so audio loads take the hardware path.
    static void* operator new(size_t bytes) {
        void* memory = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        configASSERT(memory);
        return memory;
    }
    static void operator delete(void* memory) { heap_caps_free(memory); }
    explicit DSPBlock(const DSPBlockInfo_t block_info) : info_(block_info) {};
    virtual ~DSPBlock() = default;
    virtual void process(float* buffer, size_t n_samples) = 0;
    virtual bool focus_acquire() { return false; }
    virtual void focus_release() {}
    virtual void focus_on_encoder_step(int direction) { (void) direction; }
    virtual bool focus_on_button_short() { return false; }

    DSPBlockInfo_t info() const { return info_; }
    void enable() { enabled = true; publish(); }
    void disable() { enabled = false; publish(); }
    virtual size_t parameter_count() const { return 0; }
    virtual DSPParameter_t parameter(size_t index) const { (void) index; return {}; }
    virtual int selected_parameter() const { return -1; }
    bool acquire_focus() { focused_ = focus_acquire(); publish(); return focused_; }
    void release_focus() { focus_release(); focused_ = false; publish(); }
    void encoder_step(int direction) { focus_on_encoder_step(direction); publish(); }
    bool button_short() { bool keep = focus_on_button_short(); publish(); return keep; }
    void register_changed_cb(int slot, DSPBlockChangedCallback cb, void* ctx);
    void publish() const;

    bool is_enabled() const { return enabled; }
protected:
    DSPBlockInfo_t info_;
    std::atomic<bool> enabled{true};
private:
    bool focused_ = false; // Accessed only by serialized control callbacks.
    int slot_ = -1;
    DSPBlockChangedCallback changed_cb_ = nullptr;
    void* changed_ctx_ = nullptr;
};

class BassboardDSP {
public:
    explicit BassboardDSP(int sample_rate, int max_blocks);
    ~BassboardDSP();
    void init(int16_t* input, int16_t* output, int frame_length);
    void register_data_ready_cb(void (*callback)(int));

    int capacity() const { return max_blocks; }
    void register_block_changed_cb(DSPBlockChangedCallback cb, void* ctx);
    bool add_block(DSPBlock_t block);
    bool reset_chain(bool override_enabled=false);
    bool has_block(int index) const;
    bool is_block_enabled(int index) const;
    bool set_block_enabled(int index, bool enabled_state);
    bool toggle_block_enabled(int index);
    DSPBlock* get_block(int index) const;

    void enable();
    void disable(){ enabled = false; }

    Codec *codec;
    static TaskHandle_t get_dsp_task_handle();
private:
    static std::atomic<bool> enabled;
    int sample_rate;
    static int16_t* input_buffer;
    static int16_t* output_buffer;
    static int frame_length;
    static int num_blocks;
    int max_blocks;
    DSPBlock** chain = nullptr;
    DSPBlockChangedCallback block_changed_cb = nullptr;
    void* block_changed_ctx = nullptr;
    //void enable_FFT();
    static void chain_task(void* pvParameters);
    void (*data_ready_cb)(int frame_start);
};


#endif //BASSBOARD_DSP_H

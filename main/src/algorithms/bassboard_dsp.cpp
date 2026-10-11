//
// Created by Clas Zachrisson on 2025-04-18.
//
#include "esp_log.h"
#include "bassboard_dsp.h"
#include <cstring>
// #include <sys/param.h>
#include <rom/ets_sys.h>

#include "dsp_overdrive.h"
#include "dsp_delay.h"
#include "dsp_chorus.h"
#include "dsp_phaser.h"
#include "dsp_flanger.h"
#include "dsp_compressor.h"
#include "dsp_tremolo.h"
#include <algorithm>
#include "led_driver.h"

static constexpr char TAG[] = "DSP";
static constexpr int block_to_led[5] = {3, 4, 0, 1, 2};

static TaskHandle_t dsp_chain_task_handle = nullptr;
int16_t* BassboardDSP::input_buffer = nullptr;
int16_t* BassboardDSP::output_buffer = nullptr;
int BassboardDSP::frame_length = 0;
int BassboardDSP::num_blocks = 0;
std::atomic<bool> BassboardDSP::enabled{false};

BassboardDSP::BassboardDSP(
    const int sample_rate,
    const int max_blocks) :
    sample_rate(sample_rate),
    max_blocks(std::max(1, max_blocks))
{
    data_ready_cb = nullptr;
    chain = new DSPBlock*[this->max_blocks]{};
}
BassboardDSP::~BassboardDSP()
{
    reset_chain(true);
    delete[] chain;
}


void BassboardDSP::init(int16_t* input, int16_t* output, int frame_length)
{
    input_buffer = input;
    output_buffer = output;
    BassboardDSP::frame_length = frame_length;
    num_blocks = 0;
    xTaskCreate(chain_task, "dsp_chain_task", 8192, this, 10, &dsp_chain_task_handle);
}
void BassboardDSP::enable()
{
    if (enabled)
    {
        ESP_LOGW(TAG, "Could not enable DSP: already enabled");
        return;
    }
    enabled = true;
}

TaskHandle_t BassboardDSP::get_dsp_task_handle()
{
    return dsp_chain_task_handle;
}

void BassboardDSP::register_data_ready_cb(void (*callback)(int))
{
    data_ready_cb = callback;
}


void BassboardDSP::chain_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Chain task started");
    auto self = static_cast<BassboardDSP*>(pvParameters);
    // ets_printf("chain task started: %p, %p\n", self->data_ready_cb, self);
    auto chain = self->chain;
    uint32_t frame_start = 0;
    size_t frame_size = frame_length*sizeof(int16_t);
    float dsp_buffer[frame_length];
    while (!enabled)
    {
        ESP_LOGW(TAG, "Chain task waiting to run");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    // ets_printf("loop started: %p, %p\n", self->data_ready_cb, self);
    ESP_LOGI(TAG, "Chain task loop starting");
    while (enabled)
    {
        xTaskNotifyWait(0, 0xFFFFFFFF, &frame_start, portMAX_DELAY);
        int16_t* buf = &input_buffer[frame_start*frame_length];
        for (int i=0; i<frame_length; i+=2)
        {
            dsp_buffer[i]   = buf[i+1] * (1.0f / 32768.0f);
            dsp_buffer[i+1] = buf[i]   * (1.0f / 32768.0f);
        }
        for (int i=0;i<num_blocks;i++)
        {
            if (chain[i] && chain[i]->is_enabled())
            {
                chain[i]->process(dsp_buffer, frame_length);
            }
        }
        for (int i=0; i<frame_length; i+=2)
        {
            // Swap back and convert to int16
            buf[i]   = static_cast<int16_t>(std::clamp(dsp_buffer[i+1] * 32768.0f, -32768.0f, 32767.0f));
            buf[i+1] = static_cast<int16_t>(std::clamp(dsp_buffer[i]   * 32768.0f, -32768.0f, 32767.0f));
        }
        memcpy(&output_buffer[frame_start*frame_length], buf, frame_size);
        (*self->data_ready_cb)(frame_start);
    }
}

bool BassboardDSP::add_block(DSPBlock_t block)
{
    if (enabled)
    {
        ESP_LOGE(TAG, "Could not add block: disable DSP first");
        return false;
    }
    if (num_blocks >= max_blocks)
    {
        ESP_LOGE(TAG, "Could not add block: block limit reached");
        return false;
    }

    switch (block)
    {
        case DSP_BLOCK_OVERDRIVE:
        {
            chain[num_blocks] = new DSPOverdrive(frame_length);
            break;
        }
        case DSP_BLOCK_DELAY:
        {
            chain[num_blocks] = new DSPDelay(sample_rate);
            break;
        }
        case DSP_BLOCK_CHORUS:
        {
            chain[num_blocks] = new DSPChorus;
            break;
        }
        case DSP_BLOCK_PHASER:
        {
            chain[num_blocks] = new DSPPhaser;
            break;
        }
        case DSP_BLOCK_FLANGER:
        {
            chain[num_blocks] = new DSPFlanger;
            break;
        }
        case DSP_BLOCK_COMPRESSOR:
        {
            chain[num_blocks] = new DSPCompressor;
            break;
        }
        case DSP_BLOCK_TREMOLO:
        {
            chain[num_blocks] = new DSPTremolo(sample_rate);
            break;
        }
        default:
            ESP_LOGW(TAG, "Unknown block type %d", block);
            return false;
    }
    if (num_blocks < 5)
        led_set_state(block_to_led[num_blocks], chain[num_blocks]->is_enabled() ? LED_ON : LED_ONESHOT);
    chain[num_blocks]->register_changed_cb(num_blocks, block_changed_cb, block_changed_ctx);
    num_blocks++;
    return true;
}

bool BassboardDSP::reset_chain(bool override_enabled)
{
    if (!override_enabled && enabled) return false;
    ESP_LOGI(TAG, "Resetting DSP chain");
    for (size_t i = 0; i < num_blocks; i++)
    {
        delete chain[i];
        chain[i] = nullptr;
        if (block_changed_cb) block_changed_cb(i, DSPBlockSnapshot{}, block_changed_ctx);
    }
    num_blocks = 0;
    return true;
}

bool BassboardDSP::has_block(const int index) const
{
    return index >= 0 && index < num_blocks && chain[index] != nullptr;
}

bool BassboardDSP::is_block_enabled(const int index) const
{
    if (!has_block(index))
    {
        return false;
    }
    return chain[index]->is_enabled();
}

bool BassboardDSP::set_block_enabled(const int index, const bool enabled_state)
{
    if (!has_block(index))
    {
        return false;
    }
    if (enabled_state)
    {
        chain[index]->enable();
    }
    else
    {
        chain[index]->disable();
    }
    return true;
}

bool BassboardDSP::toggle_block_enabled(const int index)
{
    if (!has_block(index))
    {
        return false;
    }
    return set_block_enabled(index, !chain[index]->is_enabled());
}

DSPBlock* BassboardDSP::get_block(const int index) const
{
    if (!has_block(index))
    {
        return nullptr;
    }
    return chain[index];
}

void DSPBlock::register_changed_cb(int slot, DSPBlockChangedCallback cb, void* ctx)
{
    slot_ = slot;
    changed_cb_ = cb;
    changed_ctx_ = ctx;
    publish();
}

void DSPBlock::publish() const
{
    if (!changed_cb_) return;
    DSPBlockSnapshot snapshot;
    snapshot.info = info_;
    snapshot.present = true;
    snapshot.enabled = is_enabled();
    snapshot.focused = focused_;
    snapshot.selected_parameter = selected_parameter();
    snapshot.parameter_count = std::min(parameter_count(), DSP_MAX_PARAMETERS);
    for (size_t i = 0; i < snapshot.parameter_count; ++i) snapshot.parameters[i] = parameter(i);
    changed_cb_(slot_, snapshot, changed_ctx_);
}

void BassboardDSP::register_block_changed_cb(DSPBlockChangedCallback cb, void* ctx)
{
    // Register before enabling the chain/input callbacks.
    block_changed_cb = cb;
    block_changed_ctx = ctx;
    for (int i = 0; i < num_blocks; ++i) chain[i]->register_changed_cb(i, cb, ctx);
}

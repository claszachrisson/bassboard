//
// Created by Clas Zachrisson on 2025-04-18.
//
#include "codec.h"
#include <array>
#include <cstring>
#include <rom/ets_sys.h>
#include <algorithm>
#include <bassboard_dsp.h>

#include "esp_attr.h"

static constexpr char TAG[] = "Codec";

i2s_chan_handle_t Codec::tx_handle;
i2s_chan_handle_t Codec::rx_handle;

constexpr int DMA_FRAME_SIZE = 128; // samples
constexpr int DMA_FRAME_SIZE_BYTES = DMA_FRAME_SIZE * sizeof(int16_t);
static int16_t input_buffer[DMA_FRAME_SIZE*2] = {};
static int16_t output_buffer[DMA_FRAME_SIZE*2] = {};
static volatile int ready_frame = 0;
static volatile int notify_frame = 0;
static volatile int output_ready_frame = 0;
volatile int Codec::output_frame = 0;

TaskHandle_t Codec::dsp_task_handle = nullptr;

Codec::Codec()
    : I2CDevice(0x18, "Codec"),
    sample_rate(44100)
{
    ioex_set_direction(EXIO_NUM_01, IOEX_OUTPUT);
    ioex_set_level(EXIO_NUM_01, 0);

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 2;
    chan_cfg.dma_frame_num = DMA_FRAME_SIZE;
    i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(this->sample_rate),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_33,
            .ws = GPIO_NUM_25,
            .dout = GPIO_NUM_26,
            .din = GPIO_NUM_27,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_APLL;
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    i2s_channel_init_std_mode(rx_handle, &std_cfg);
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
    i2s_channel_init_std_mode(tx_handle, &std_cfg);

    i2s_event_callbacks_t rx_callbacks = {
        .on_recv = i2s_rx_cb,
    };
    i2s_event_callbacks_t tx_callbacks = {
        .on_sent = i2s_tx_sent_cb,
    };
    i2s_channel_register_event_callback(rx_handle, &rx_callbacks, nullptr);
    i2s_channel_register_event_callback(tx_handle, &tx_callbacks, nullptr);

    i2s_channel_enable(tx_handle);
    i2s_channel_enable(rx_handle);

    ioex_set_level(EXIO_NUM_01, 1);

    const std::array<i2c_pair, 39> init_sequence = {{
        {0x00, 0x00}, // pg 0
        {0x01, 0x01}, // sw reset
        {0x04, 0x07}, // PLL_CLKIN = BCLK, CODEC_CLKIN = PLL_CLK
        {0x05, 0x16}, // PLL P=1, R=6, PLL powered down
        {0x06, 0x0a}, // PLL J=10
        {0x07, 0x00}, // PLL D MSB = 0
        {0x08, 0x00}, // PLL D LSB = 0
        {0x05, 0x96}, // PLL powered on
        {0x12, 0x83}, // NADC 3, powered on
        {0x13, 0x8a}, // MADC 10, powered on
        {0x14, 0x40}, // AOSR_VAL = 64
        {0x0b, 0x83}, // NDAC 3
        {0x0c, 0x8a}, // MDAC 10
        {0x0d, 0x00}, // DOSR_VAL MSB
        {0x0d, 0x40}, // DOSR_VAL LSB = 64
        {0x1b, 0xc0}, // LJ audio, 16-bit
        {0x3d, 0x0a}, // PRB_R10
        {0x3c, 0x0a}, // PRB_P10
        {0x00, 0x08}, // pg 8
        {0x01, 0x04}, // Adaptive filtering enabled in DAC
        {0x00, 0x00}, // pg 0
        {0x00, 0x01}, // pg 1
        {0x1f, 0x04}, // HPOUT CM 1V35
        {0x21, 0x4e}, // Depop
        {0x23, 0x80}, // DAC to HPOUT
        {0x28, 0x06}, // Unmute HPOUT, 0 dB
        {0x2c, 0x04}, // HPOUT in Line out mode
        {0x24, 0x00}, // HPOUT analog gain 0 dB
        {0x1f, 0x86}, // HPOUT power up
        {0xFF, 0xFF}, // Wait
        {0x30, 0x80}, // MIC1LP, 20kOhm
        {0x31, 0x80}, // CM, 20kOhm
        //{0x2f, 0x0F}, // PGA gain ???
        {0x00, 0x00}, // pg 0
        {0x51, 0x80}, // power up ADC
        {0x3f, 0xb4}, // Power up DAC, right data
        {0x41, 0x00}, // DAC gain 0 dB
        {0xFF, 0xFF}, // Wait
        {0x52, 0x00}, // unmute ADC
        {0x40, 0x04}, // Unmute DAC
    }};

    this->register_write_random(init_sequence);

    // this->main_ringbuf_task_start();
    // this->start_stream_send_task();
    ESP_LOGI(TAG, "Initialised codec");
}

int Codec::get_frame_size() { return DMA_FRAME_SIZE; }
int16_t* Codec::get_input_buffer() { return input_buffer; }
int16_t* Codec::get_output_buffer() { return output_buffer; }

void Codec::set_output_frame_ready(const int frame_start)
{
    output_ready_frame = frame_start;
}
void Codec::set_dsp_task_handle(TaskHandle_t handle)
{
    dsp_task_handle = handle;
}


// void Codec::mute_output()   { transmitting = false; }
// void Codec::unmute_output() { transmitting = true;  }

IRAM_ATTR bool Codec::i2s_rx_cb(i2s_chan_handle_t, i2s_event_data_t *event, void*)
{
    if (!dsp_task_handle) return false;
    if (event->size != DMA_FRAME_SIZE_BYTES) { ets_printf("i2s rx cb size mismatch\n"); return false; }
    // ets_printf("i2s rx cb, copying, ready_frame, DMA_FRAME_SIZE, event size: %d, %d, %d \n", ready_frame, DMA_FRAME_SIZE, event->size);
    memcpy(&input_buffer[ready_frame*DMA_FRAME_SIZE], event->dma_buf, DMA_FRAME_SIZE_BYTES);

    notify_frame = ready_frame;
    ready_frame = !ready_frame;

    BaseType_t hp = pdFALSE;
    xTaskNotifyFromISR(dsp_task_handle, notify_frame, eSetValueWithOverwrite, &hp);
    // ets_printf("i2s rx cb, notified \n");
    return hp == pdTRUE;
}

IRAM_ATTR bool Codec::i2s_tx_sent_cb(i2s_chan_handle_t, i2s_event_data_t* event, void*)
{
    memcpy(event->dma_buf, &output_buffer[output_ready_frame*DMA_FRAME_SIZE], event->size);
    // output_ready_buffer = !output_ready_buffer; may be set by whatever writes to output buffer
    return false;
}

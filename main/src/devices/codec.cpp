//
// Created by Clas Zachrisson on 2025-04-18.
//
#include "codec.h"
#include <array>
#include <cstring>
#include <rom/ets_sys.h>
#include <algorithm>

#include "esp_attr.h"

i2s_chan_handle_t Codec::tx_handle;
i2s_chan_handle_t Codec::rx_handle;

constexpr int DMA_FRAME_SIZE = 120; // samples
constexpr int STREAMBUF_N_FRAMES = 3;
constexpr size_t STREAMBUF_SIZE = DMA_FRAME_SIZE * STREAMBUF_N_FRAMES * sizeof(uint16_t);

TaskHandle_t Codec::stream_task_handle = nullptr;
StreamBufferHandle_t xStreamBuffer;
StaticStreamBuffer_t StreamBufferStruct;
static uint8_t ucStorageBuffer[STREAMBUF_SIZE];

bool Codec::transmitting = true;

void consume_frame()
{
    uint16_t junk[DMA_FRAME_SIZE];
    xStreamBufferReceiveFromISR(xStreamBuffer, junk, sizeof(junk), nullptr);
}

Codec::Codec(IOExpander* io_expander)
    : I2CDevice(0x18, "Codec"),
    io_expander(io_expander),
    sample_rate(44100)
{
    xStreamBuffer = xStreamBufferCreateStatic(
        sizeof(ucStorageBuffer),
        1,
        ucStorageBuffer,
        &StreamBufferStruct
    );
    io_expander->set_direction(EXIO_NUM_01, IOExpander::OUTPUT);
    io_expander->set_level(EXIO_NUM_01, 0);

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

    io_expander->set_level(EXIO_NUM_01, 1);

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
}

int Codec::get_frame_size() { return DMA_FRAME_SIZE; }

void Codec::mute_output()   { transmitting = false; }
void Codec::unmute_output() { transmitting = true;  }

IRAM_ATTR bool Codec::i2s_rx_cb(i2s_chan_handle_t, i2s_event_data_t *event, void*)
{
    // auto samples = static_cast<uint16_t*>(event->dma_buf);
    // for (int i=0; i<event->size; i++)
    // {
    //     if (samples[i] & 0x80) ets_printf("doozy");
    // }
    if (event->size % DMA_FRAME_SIZE != 0) { ets_printf("recv only %d samples\n", event->size); return false; }
    if (event->size > xStreamBufferSpacesAvailable(xStreamBuffer))
    {
        // ets_printf("not enough space in buffer\n");
        consume_frame();
    }

    size_t bytes = xStreamBufferSendFromISR(
        xStreamBuffer,
        event->dma_buf,
        event->size,
        nullptr
    );

    if (bytes & 1) {ets_printf("sent odd number of bytes"); return false; }
    // if (bytes != event->size) {
    //     ets_printf("not enough space in buffer\n");
    // }

    return false;
}

void Codec::read(int16_t* buffer, int frames)
{
    for (int f = 0; f < frames; f++)
    {
        streamBufferReceive(&buffer[f * DMA_FRAME_SIZE], DMA_FRAME_SIZE*sizeof(int16_t), portMAX_DELAY);
        for (int i = 0; i < frames*DMA_FRAME_SIZE; i+=2)
        {
            std::swap(buffer[f*DMA_FRAME_SIZE + i], buffer[f*DMA_FRAME_SIZE + i + 1]);
        }
    }
}
void Codec::read(float* buffer, int frames)
{
    constexpr float scale = 1.0f / 32768.0f;
    int16_t tmp[DMA_FRAME_SIZE];
    for (int f = 0; f < frames; ++f)
    {
        streamBufferReceive(tmp, DMA_FRAME_SIZE*sizeof(int16_t), portMAX_DELAY);
        for (int i = 0; i < DMA_FRAME_SIZE; i += 2)
        {
            buffer[f * DMA_FRAME_SIZE + i]     = tmp[i+1] * scale;
            buffer[f * DMA_FRAME_SIZE + i + 1] = tmp[i]   * scale;
        }
    }
}

size_t Codec::streamBufferReceive(void* buffer, size_t size, TickType_t ticks_to_wait=portMAX_DELAY)
{
    return xStreamBufferReceive(xStreamBuffer, buffer, size, ticks_to_wait);
}

IRAM_ATTR bool Codec::i2s_tx_sent_cb(i2s_chan_handle_t, i2s_event_data_t* event, void*)
{
    if (!transmitting) return false;

    streamBufferReceive(
        event->dma_buf,
        event->size,
        0
    );
    return false;
}
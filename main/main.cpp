#include "led_driver.h"
#include "io_expander.h"
#include "encoder.h"
#include "codec.h"
//#include "a2dp_source.h"
// #include <rom/ets_sys.h>

#include "lcd.h"
#include "freertos/semphr.h"
#include "bassboard_dsp.h"
#include "lib_i2c.h"
#include "esp_log.h"
// #include "pitchyin.h"


/* I2C Addreses ----
 * EEPROM:      0x50
 * LED Driver:  0x45
 * IO Expander: 0x20
 * Codec:       0x18
*/

static constexpr char TAG[] = "MAIN";
static constexpr int BLOCK_COUNT = 5;

static BassboardDSP* g_dsp = nullptr;
static int g_focus_block = -1;
static DSPBlock* g_focus_owner = nullptr;
static SemaphoreHandle_t g_control_mutex = nullptr;
struct ControlGuard {
    ControlGuard() { xSemaphoreTake(g_control_mutex, portMAX_DELAY); }
    ~ControlGuard() { xSemaphoreGive(g_control_mutex); }
};

static portMUX_TYPE g_focus_mux = portMUX_INITIALIZER_UNLOCKED;

static int block_index_from_switch(const int port, const int bit)
{
    if (port == 1 && bit == 0) return 0;
    if (port == 1 && bit == 1) return 1;
    if (port == 0 && bit == 5) return 2;
    if (port == 0 && bit == 6) return 3;
    if (port == 0 && bit == 7) return 4;
    return -1;
}

static int led_index_from_block(const int block_idx)
{
    // LED order by switch: LED0=SW3, LED1=SW4, LED2=SW5, LED3=SW1, LED4=SW2
    // Block order by switch: block0=SW1, block1=SW2, block2=SW3, block3=SW4, block4=SW5
    static constexpr int block_to_led[BLOCK_COUNT] = {3, 4, 0, 1, 2};
    if (block_idx < 0 || block_idx >= BLOCK_COUNT)
    {
        return -1;
    }
    return block_to_led[block_idx];
}

static void update_block_led(const int block_idx)
{
    if (block_idx < 0 || block_idx >= BLOCK_COUNT || !g_dsp)
    {
        return;
    }
    const int led_idx = led_index_from_block(block_idx);
    if (led_idx < 0)
    {
        return;
    }
    if (!g_dsp->has_block(block_idx))
    {
        led_set_state(led_idx, LED_OFF);
        return;
    }
    led_set_state(led_idx, g_dsp->is_block_enabled(block_idx) ? LED_ON : LED_ONESHOT);
}

static int get_focus_block()
{
    int focus = -1;
    portENTER_CRITICAL(&g_focus_mux);
    focus = g_focus_block;
    portEXIT_CRITICAL(&g_focus_mux);
    return focus;
}

static void clear_focus()
{
    int prev_focus = -1;
    DSPBlock* prev_owner = nullptr;
    portENTER_CRITICAL(&g_focus_mux);
    prev_focus = g_focus_block;
    prev_owner = g_focus_owner;
    g_focus_block = -1;
    g_focus_owner = nullptr;
    portEXIT_CRITICAL(&g_focus_mux);

    if (prev_owner)
    {
        prev_owner->release_focus();
    }
    if (prev_focus != -1)
    {
        update_block_led(prev_focus);
    }
}

static bool try_claim_focus(const int block_idx)
{
    if (!g_dsp)
    {
        return false;
    }
    DSPBlock* owner = g_dsp->get_block(block_idx);
    if (!owner || !owner->acquire_focus())
    {
        return false;
    }

    portENTER_CRITICAL(&g_focus_mux);
    g_focus_block = block_idx;
    g_focus_owner = owner;
    portEXIT_CRITICAL(&g_focus_mux);

    const int led_idx = led_index_from_block(block_idx);
    if (led_idx >= 0)
    {
        led_set_state(led_idx, LED_PWM0);
    }
    return true;
}

static void on_switch_event(const int port, const int bit, const ioex_switch_event_type type, const uint32_t press_ms, void*)
{
    ControlGuard guard;
    (void) press_ms;
    const int block_idx = block_index_from_switch(port, bit);
    const int focus = get_focus_block();

    if (block_idx >= 0)
    {
        if (type == IOEX_SWITCH_LONG)
        {
            if (focus == -1 && try_claim_focus(block_idx))
            {
                ESP_LOGI(TAG, "Focus claimed by block %d", block_idx);
            }
            return;
        }

        if (type == IOEX_SWITCH_SHORT && focus == -1 && g_dsp && g_dsp->has_block(block_idx))
        {
            g_dsp->toggle_block_enabled(block_idx);
            update_block_led(block_idx);
            ESP_LOGI(TAG, "Block %d toggled -> %s", block_idx, g_dsp->is_block_enabled(block_idx) ? "ENABLED" : "DISABLED");
            return;
        }
    }

    if (port == 0 && bit == 4)
    {
        if (type == IOEX_SWITCH_SHORT && focus != -1)
        {
            DSPBlock* owner = nullptr;
            portENTER_CRITICAL(&g_focus_mux);
            owner = g_focus_owner;
            portEXIT_CRITICAL(&g_focus_mux);

            if (owner && !owner->button_short())
            {
                clear_focus();
                ESP_LOGI(TAG, "Focus released from block %d", focus);
            }
            return;
        }

        if (type == IOEX_SWITCH_LONG && focus != -1)
        {
            clear_focus();
            ESP_LOGI(TAG, "Focus released from block %d", focus);
        }
    }
}

static void on_encoder_step(const int direction, const int32_t position, void*)
{
    ControlGuard guard;
    (void) position;
    const int focus = get_focus_block();
    if (focus == -1)
    {
        return;
    }

    DSPBlock* owner = nullptr;
    portENTER_CRITICAL(&g_focus_mux);
    owner = g_focus_owner;
    portEXIT_CRITICAL(&g_focus_mux);

    if (!owner)
    {
        return;
    }
    owner->encoder_step(direction);
}

extern "C" void app_main(void)
{
    g_control_mutex = xSemaphoreCreateMutex();
    configASSERT(g_control_mutex);

    i2c0_init(GPIO_NUM_14,GPIO_NUM_13);
    ioex_init();
    led_init();
    // ESP_LOGI("main", "dsp task handle %p ", BassboardDSP::get_dsp_task_handle());
    led_set_state(0, LED_PWM0);
    led_set_state(1, LED_PWM0);
    led_set_state(2, LED_PWM0);
    led_set_state(3, LED_PWM0);
    led_set_state(4, LED_PWM0);
    ioex_register_switch(EXIO_NUM_04, true);
    ioex_register_switch(EXIO_NUM_05, true);
    ioex_register_switch(EXIO_NUM_06, true);
    ioex_register_switch(EXIO_NUM_07, true);
    ioex_register_switch(EXIO_NUM_10, true);
    ioex_register_switch(EXIO_NUM_11, true);
    static Encoder encoder;


    static Codec codec;

    static BassboardDSP dsp(44100, BLOCK_COUNT);
    static LCD display(dsp.capacity());
    dsp.register_block_changed_cb(LCD::block_changed, &display);
    g_dsp = &dsp;
    dsp.codec = &codec;
    dsp.init(
        Codec::get_input_buffer(),
        Codec::get_output_buffer(),
        Codec::get_frame_size()
    );
    Codec::set_dsp_task_handle(BassboardDSP::get_dsp_task_handle());
    dsp.register_data_ready_cb(Codec::set_output_frame_ready);
    // ets_printf("Codec DTR 1: %p\n", Codec::set_output_frame_ready);
    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        led_set_state(i, LED_OFF);
    }
    dsp.add_block(DSP_BLOCK_OVERDRIVE);
    dsp.add_block(DSP_BLOCK_TREMOLO);
    dsp.enable();
    ioex_register_switch_event_cb(on_switch_event, nullptr);
    encoder.register_step_cb(on_encoder_step, nullptr);
    // ets_printf("Codec DTR 2: %p\n", Codec::set_output_frame_ready);
    // ets_printf("dsp main p: %p\n", &dsp);
    // ets_printf("dsp main dtr p: %p\n", dsp.data_ready_cb);

    // int frames = 12;
    // int yin_num_samples = frames * Codec::get_frame_size();
    // auto* yin_samples = static_cast<float*>(malloc(yin_num_samples * sizeof(float)));
    // Yin yin(44100, yin_num_samples);
    // Codec::mute_output();
    // while (true)
    // {
    //     // ESP_LOGI("MAIN", "Fetching samples");
    //     // float yin_samples[yin_num_samples];
    //     codec.read(yin_samples, frames);
    //     // ESP_LOGI("MAIN", "Running Yin");
    //     yin.getPitch(yin_samples);
    //     // return;
    // }

    //LCD lcd(&IOExp);

    /*A2DPSource a2dp_source;
    a2dp_source.register_a2d_source_data_cb(&codec.a2dp_data_cb_nonblocking);
    a2dp_source.init();*/
    /*int state;
    while (true)
    {
        if ((state = a2dp_source.get_state()) ==5)
            break;
        ESP_LOGI("MAIN", "A2DP Source state: %d", state);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    };
    a2dp_source.start();*/

    /* BBDSP bbdsp(44100);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    uint32_t len = 16384;
    auto* buf = static_cast<uint8_t*>(heap_caps_aligned_alloc(4,len, MALLOC_CAP_DMA));
    codec.i2s_read_blocking(buf, len);

    bbdsp.estimate_pitch(reinterpret_cast<uint16_t*>(buf), len>>1); */
}

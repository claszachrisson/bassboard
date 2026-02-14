#include "led_driver.h"
#include "io_expander.h"
#include "codec.h"
//#include "a2dp_source.h"
#include "lcd.h"
#include "bassboard_dsp.h"
#include "i2c_device.h"
#include "pitchyin.h"


/*
 * EEPROM:      0x58
 * LED Driver:  0x45
 * IO Expander: 0x20
 * Codec:       0x18
*/

extern "C" void app_main(void)
{
    i2c0_init(GPIO_NUM_14,GPIO_NUM_13);

    LEDDriver LEDs;

    // LEDs.set_state(0, LED_Driver::LED_ON);
    // LEDs.set_state(1, LED_Driver::LED_ON);
    // LEDs.set_state(2, LED_Driver::LED_ON);
    // LEDs.set_state(3, LEDDriver::LED_PWM0);
    //LEDs.set_state(4, LEDDriver::LED_ON);
    static IOExpander IOExp;

    static Codec codec(&IOExp);

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
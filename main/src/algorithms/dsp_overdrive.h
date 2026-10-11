//
// Created by Clas Zachrisson on 2026-02-14.
//

#ifndef DSP_OVERDRIVE_H
#define DSP_OVERDRIVE_H

#include "bassboard_dsp.h"
#include "esp_dsp.h"

static constexpr int COEFFS_LEN = 51;

class DSPOverdrive : public DSPBlock {
public:
    explicit DSPOverdrive(int frame_length);
    ~DSPOverdrive() override;
    size_t parameter_count() const override { return 2; }
    DSPParameter_t parameter(size_t index) const override;
    int selected_parameter() const override { return static_cast<int>(focus_target); }
    void process(float* buffer, size_t n_samples) override;
    bool focus_acquire() override;
    void focus_release() override;
    void focus_on_encoder_step(int direction) override;
    bool focus_on_button_short() override;

    std::atomic<float> pre_gain{5.0f};
    std::atomic<float> post_gain{.2f};
private:
    enum FocusTarget
    {
        FOCUS_TARGET_PRE_GAIN,
        FOCUS_TARGET_POST_GAIN,
    };
    FocusTarget focus_target = FOCUS_TARGET_PRE_GAIN;
    fir_f32_t fir = {};
    float delay_arr[COEFFS_LEN+4] = {};
    float* prefilter_buffer = nullptr;
    int prefilter_len = 0;
};


#endif //DSP_OVERDRIVE_H

//
// Created by Codex on 2026-03-04.
//

#ifndef DSP_TREMOLO_H
#define DSP_TREMOLO_H

#include "bassboard_dsp.h"

class DSPTremolo : public DSPBlock {
public:
    explicit DSPTremolo(int sample_rate);
    ~DSPTremolo() override = default;
    size_t parameter_count() const override { return 2; }
    DSPParameter_t parameter(size_t index) const override;
    int selected_parameter() const override { return static_cast<int>(focus_target); }
    void process(float* buffer, size_t n_samples) override;
    bool focus_acquire() override;
    void focus_release() override;
    void focus_on_encoder_step(int direction) override;
    bool focus_on_button_short() override;

    // LFO frequency in Hz
    std::atomic<float> rate_hz{10.0f};
    // 0.0 = no modulation, 1.0 = full modulation to silence during carrier low
    std::atomic<float> depth{1.0f};

private:
    enum FocusTarget
    {
        FOCUS_TARGET_RATE,
        FOCUS_TARGET_DEPTH,
    };
    int sample_rate_hz = 44100;
    float lfo_phase = 0.0f;
    FocusTarget focus_target = FOCUS_TARGET_RATE;
};

#endif //DSP_TREMOLO_H

//
// Created by Codex on 2026-03-04.
//

#ifndef DSP_DELAY_H
#define DSP_DELAY_H

#include <vector>

#include "bassboard_dsp.h"

class DSPDelay : public DSPBlock {
public:
    explicit DSPDelay(int sample_rate);
    ~DSPDelay() override = default;
    size_t parameter_count() const override { return 4; }
    DSPParameter_t parameter(size_t index) const override;
    void process(float* buffer, size_t n_samples) override;

    // 0.0 = dry only, 1.0 = wet only
    std::atomic<float> dry_wet{0.35f};
    // Feedback gain applied to delayed sample fed back into delay line
    std::atomic<float> feedback{0.35f};
    // Delay time in milliseconds
    std::atomic<float> time_ms{280.0f};
    // Rate parameter placeholder for future tempo/mod sync handling
    std::atomic<float> rate_hz{1.0f};

private:
    int sample_rate_hz = 44100;
    std::vector<float> delay_line;
    size_t write_idx = 0;
};

#endif //DSP_DELAY_H

//
// Created by Codex on 2026-03-04.
//

#ifndef DSP_FLANGER_H
#define DSP_FLANGER_H

#include "bassboard_dsp.h"

class DSPFlanger : public DSPBlock {
public:
    DSPFlanger() : DSPBlock({"Flanger", "FLAN"}) {}
    ~DSPFlanger() override = default;
    size_t parameter_count() const override { return 4; }
    DSPParameter_t parameter(size_t index) const override;
    void process(float* buffer, size_t n_samples) override;

    std::atomic<float> depth{0.35f};
    std::atomic<float> rate_hz{0.3f};
    std::atomic<float> feedback{0.2f};
    std::atomic<float> mix{0.4f};
};

#endif //DSP_FLANGER_H

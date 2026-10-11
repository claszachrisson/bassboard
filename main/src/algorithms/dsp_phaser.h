//
// Created by Codex on 2026-03-04.
//

#ifndef DSP_PHASER_H
#define DSP_PHASER_H

#include "bassboard_dsp.h"

class DSPPhaser : public DSPBlock {
public:
    DSPPhaser() : DSPBlock({"Phaser", "PHAS"}) {}
    ~DSPPhaser() override = default;
    size_t parameter_count() const override { return 3; }
    DSPParameter_t parameter(size_t index) const override;
    void process(float* buffer, size_t n_samples) override;

    std::atomic<float> depth{0.5f};
    std::atomic<float> rate_hz{0.5f};
    std::atomic<float> feedback{0.25f};
};

#endif //DSP_PHASER_H

//
// Created by Codex on 2026-03-04.
//

#ifndef DSP_CHORUS_H
#define DSP_CHORUS_H

#include "bassboard_dsp.h"

class DSPChorus : public DSPBlock {
public:
    DSPChorus() : DSPBlock({"Chorus", "CHOR"}) {}
    ~DSPChorus() override = default;
    size_t parameter_count() const override { return 3; }
    DSPParameter_t parameter(size_t index) const override;
    void process(float* buffer, size_t n_samples) override;

    std::atomic<float> depth{0.25f};
    std::atomic<float> rate_hz{0.8f};
    std::atomic<float> mix{0.35f};
};

#endif //DSP_CHORUS_H

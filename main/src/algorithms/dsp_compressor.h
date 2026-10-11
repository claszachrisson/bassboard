//
// Created by Codex on 2026-03-04.
//

#ifndef DSP_COMPRESSOR_H
#define DSP_COMPRESSOR_H

#include "bassboard_dsp.h"

class DSPCompressor : public DSPBlock {
public:
    DSPCompressor() : DSPBlock({"Compressor", "COMP"}) {}
    ~DSPCompressor() override = default;
    size_t parameter_count() const override { return 5; }
    DSPParameter_t parameter(size_t index) const override;
    void process(float* buffer, size_t n_samples) override;

    std::atomic<float> threshold_db{-18.0f};
    std::atomic<float> ratio{4.0f};
    std::atomic<float> attack_ms{10.0f};
    std::atomic<float> release_ms{120.0f};
    std::atomic<float> makeup_db{0.0f};
};

#endif //DSP_COMPRESSOR_H

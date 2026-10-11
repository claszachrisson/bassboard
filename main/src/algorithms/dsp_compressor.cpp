//
// Created by Codex on 2026-03-04.
//

#include "dsp_compressor.h"

void DSPCompressor::process(float* buffer, const size_t n_samples)
{
    (void) buffer;
    (void) n_samples;
    if (!enabled)
    {
        return;
    }
    // Skeleton: currently pass-through.
}

DSPParameter_t DSPCompressor::parameter(size_t index) const
{
    switch (index) {
        case 0: return {0, "Threshold", "dB", threshold_db.load(), 1};
        case 1: return {1, "Ratio", ":1", ratio.load(), 1};
        case 2: return {2, "Attack", "ms", attack_ms.load(), 0};
        case 3: return {3, "Release", "ms", release_ms.load(), 0};
        case 4: return {4, "Makeup", "dB", makeup_db.load(), 1};
        default: return {};
    }
}

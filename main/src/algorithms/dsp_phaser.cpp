//
// Created by Codex on 2026-03-04.
//

#include "dsp_phaser.h"

void DSPPhaser::process(float* buffer, const size_t n_samples)
{
    (void) buffer;
    (void) n_samples;
    if (!enabled)
    {
        return;
    }
    // Skeleton: currently pass-through.
}

DSPParameter_t DSPPhaser::parameter(size_t index) const
{
    switch (index) {
        case 0: return {0, "Depth", "", depth.load(), 2};
        case 1: return {1, "Rate", "Hz", rate_hz.load(), 1};
        case 2: return {2, "Feedback", "", feedback.load(), 2};
        default: return {};
    }
}

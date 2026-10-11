//
// Created by Codex on 2026-03-04.
//

#include "dsp_tremolo.h"

#include <algorithm>
#include <cmath>

static constexpr float RATE_STEP_HZ = 0.5f;
static constexpr float DEPTH_STEP = 0.05f;
static constexpr float TWO_PI = 6.2831853071795864769f;
// Larger value gives a "squarer" shape while keeping transitions smooth.
static constexpr float SQUARE_SOFTNESS = 6.0f;

DSPTremolo::DSPTremolo(const int sample_rate) : DSPBlock({"Tremolo", "TRMLO"})
{
    sample_rate_hz = std::max(sample_rate, 1);
}

void DSPTremolo::process(float* buffer, const size_t n_samples)
{
    if (!enabled)
    {
        return;
    }

    const float clamped_rate_hz = std::clamp(rate_hz.load(), 0.0f, 50.0f);
    const float clamped_depth = std::clamp(depth.load(), 0.0f, 1.0f);
    const float phase_inc = clamped_rate_hz / static_cast<float>(sample_rate_hz);

    for (size_t i = 0; i < n_samples; i++)
    {
        const float phase_rad = lfo_phase * TWO_PI;
        const float carrier = 0.5f * (1.0f + std::tanh(SQUARE_SOFTNESS * std::sinf(phase_rad)));
        const float gain = (1.0f - clamped_depth) + (clamped_depth * carrier);
        buffer[i] *= gain;

        lfo_phase += phase_inc;
        if (lfo_phase >= 1.0f)
        {
            lfo_phase -= 1.0f;
        }
    }
}

bool DSPTremolo::focus_acquire()
{
    focus_target = FOCUS_TARGET_RATE;
    return true;
}

void DSPTremolo::focus_release()
{
    focus_target = FOCUS_TARGET_RATE;
}

void DSPTremolo::focus_on_encoder_step(const int direction)
{
    if (focus_target == FOCUS_TARGET_RATE)
    {
        const float delta = direction > 0 ? RATE_STEP_HZ : -RATE_STEP_HZ;
        rate_hz = std::clamp(rate_hz + delta, 0.0f, 50.0f);
    }
    else
    {
        const float delta = direction > 0 ? DEPTH_STEP : -DEPTH_STEP;
        depth = std::clamp(depth + delta, 0.0f, 1.0f);
    }
}

bool DSPTremolo::focus_on_button_short()
{
    if (focus_target == FOCUS_TARGET_RATE)
    {
        focus_target = FOCUS_TARGET_DEPTH;
        return true;
    }
    return false;
}

DSPParameter_t DSPTremolo::parameter(size_t index) const
{
    switch (index) {
        case 0: return {0, "Rate", "Hz", rate_hz.load(), 1};
        case 1: return {1, "Depth", "", depth.load(), 2};
        default: return {};
    }
}

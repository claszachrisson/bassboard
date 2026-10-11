//
// Created by Codex on 2026-03-04.
//

#include "dsp_delay.h"

#include <algorithm>
#include <cmath>

#include "esp_log.h"

static constexpr char TAG[] = "DELAY";
static constexpr size_t MAX_DELAY_SECONDS = 2;

DSPDelay::DSPDelay(const int sample_rate) : DSPBlock({"Delay", "DELA"})
{
    sample_rate_hz = std::max(sample_rate, 1);
    delay_line.assign(static_cast<size_t>(sample_rate_hz) * MAX_DELAY_SECONDS, 0.0f);
    ESP_LOGI(TAG, "Delay initialized (%d Hz, max %.1f s)", sample_rate_hz, static_cast<double>(MAX_DELAY_SECONDS));
}

void DSPDelay::process(float* buffer, const size_t n_samples)
{
    if (!enabled || delay_line.empty())
    {
        return;
    }

    const float clamped_dry_wet = std::clamp(dry_wet.load(), 0.0f, 1.0f);
    const float clamped_feedback = std::clamp(feedback.load(), -0.99f, 0.99f);
    const float clamped_time_ms = std::clamp(time_ms.load(), 1.0f, static_cast<float>(MAX_DELAY_SECONDS * 1000));
    const size_t delay_samples = static_cast<size_t>(
        std::clamp(
            static_cast<int>(std::lround((clamped_time_ms / 1000.0f) * static_cast<float>(sample_rate_hz))),
            1,
            static_cast<int>(delay_line.size() - 1)
        )
    );

    const float dry_gain = 1.0f - clamped_dry_wet;
    const float wet_gain = clamped_dry_wet;

    for (size_t i = 0; i < n_samples; i++)
    {
        const float in = buffer[i];

        const size_t read_idx = (write_idx + delay_line.size() - delay_samples) % delay_line.size();
        const float delayed = delay_line[read_idx];

        buffer[i] = (in * dry_gain) + (delayed * wet_gain);
        delay_line[write_idx] = in + (delayed * clamped_feedback);

        write_idx++;
        if (write_idx >= delay_line.size())
        {
            write_idx = 0;
        }
    }
}

DSPParameter_t DSPDelay::parameter(size_t index) const
{
    switch (index) {
        case 0: return {0, "Mix", "", dry_wet.load(), 2};
        case 1: return {1, "Feedback", "", feedback.load(), 2};
        case 2: return {2, "Time", "ms", time_ms.load(), 0};
        case 3: return {3, "Rate", "Hz", rate_hz.load(), 1};
        default: return {};
    }
}

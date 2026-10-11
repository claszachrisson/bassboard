//
// Created by Clas Zachrisson on 2026-02-14.
//

#include "dsp_overdrive.h"
#include <algorithm>
#include <cmath>
#include "esp_log.h"

static constexpr float PREGAIN_STEP = 0.1f;
static constexpr float POSTGAIN_STEP = 0.02f;
static constexpr char TAG[] = "OVDR";

static float LPF_COEFFS[COEFFS_LEN] = {
    0.0003244128238931089, 0.0009682412055669114, -0.0004407184901975906, -0.001669791337030939,
    -0.0003169443226993921, 0.002765458929872712, 0.002046631850506671, -0.003329725018335954,
    -0.0049977801943791285, 0.0024302745203326702, 0.008771788796675952, 0.0010056836504283976,
    -0.012239844630158835, -0.007782927086263334, 0.013485906442392344, 0.01803998340782176,
    -0.009911192277429955, -0.030982869953111287, -0.0018055806287180902, 0.04487715243585647,
    0.026971801402796883, -0.057372026373616596, -0.08151846679473825, 0.06607245315405676,
    0.3093799860220564, 0.4313076703762616, 0.3093799860220564, 0.06607245315405676, -0.08151846679473825,
    -0.057372026373616596, 0.026971801402796883, 0.04487715243585647, -0.0018055806287180902,
    -0.030982869953111287, -0.009911192277429955, 0.01803998340782176, 0.013485906442392344,
    -0.007782927086263334, -0.012239844630158835, 0.0010056836504283976, 0.008771788796675952,
    0.0024302745203326702, -0.0049977801943791285, -0.003329725018335954, 0.002046631850506671,
    0.002765458929872712, -0.0003169443226993921, -0.001669791337030939, -0.0004407184901975906,
    0.0009682412055669114, 0.0003244128238931089
};

static constexpr DSPBlockInfo_t kBlockInfo = {
    "Overdrive",
    "OVDR"
};

DSPOverdrive::DSPOverdrive(const int frame_length) : DSPBlock(kBlockInfo)
{
    prefilter_len = frame_length;
    prefilter_buffer = new float[prefilter_len] {};

    if (dsps_fir_init_f32(&fir, LPF_COEFFS, delay_arr, COEFFS_LEN) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialise overdrive prefilter");
    }
    ESP_LOGI(TAG, "Overdrive initialised");
}

DSPOverdrive::~DSPOverdrive()
{
    delete[] prefilter_buffer;
}

void DSPOverdrive::process(float* buffer, size_t n_samples)
{
    // printf("Overdrive processing\n");
    // printf("od process %d %p", enabled, prefilter_buffer);
    if (!enabled || prefilter_buffer == nullptr) return;
    const float input_gain = pre_gain.load();
    const float output_gain = post_gain.load();
    const size_t process_len = std::min(n_samples, static_cast<size_t>(prefilter_len));
    dsps_fir_f32_ae32(&fir, buffer, prefilter_buffer, process_len);
    for (size_t i = 0; i < process_len; i++)
    {
        float sign = (prefilter_buffer[i] >= 0.0f) ? 1.0f : -1.0f;
        float abs_s = std::fabs(prefilter_buffer[i]) * input_gain;

        // hard clip
        // buffer[i] = std::clamp(buffer[i], -clip_value, clip_value);

        // abs_s = fminf(abs_s - abs_s*abs_s*abs_s*0.33333f, 0.6667f);
        if (abs_s < (1.0f / 3.0f))
            abs_s *= 2.0f;
        else if (abs_s < (2.0f / 3.0f))
            abs_s = (3.0f - (2.0f - 3.0f * abs_s) * (2.0f - 3.0f * abs_s)) * 0.33333f;
        else
            abs_s = 1.0f;


        buffer[i] = sign * abs_s * output_gain;
    }
}

bool DSPOverdrive::focus_acquire()
{
    focus_target = FOCUS_TARGET_PRE_GAIN;
    ESP_LOGI(TAG, "Focus acquired (target=PRE, pre_gain=%.2f, post_gain=%.2f)", static_cast<double>(pre_gain), static_cast<double>(post_gain));
    return true;
}

void DSPOverdrive::focus_release()
{
    focus_target = FOCUS_TARGET_PRE_GAIN;
    ESP_LOGI(TAG, "Focus released");
}

void DSPOverdrive::focus_on_encoder_step(const int direction)
{
    const float sign = direction > 0 ? 1.0f : -1.0f;
    if (focus_target == FOCUS_TARGET_PRE_GAIN)
    {
        pre_gain = std::clamp(pre_gain + sign*PREGAIN_STEP, 0.1f, 10.0f);
    }
    else
    {
        post_gain = std::clamp(post_gain + sign*POSTGAIN_STEP, 0.0f, 2.0f);
    }
    ESP_LOGI(
        TAG,
        "Focus edit (%s): pre_gain=%.2f post_gain=%.2f",
        focus_target == FOCUS_TARGET_PRE_GAIN ? "PRE" : "POST",
        static_cast<double>(pre_gain),
        static_cast<double>(post_gain)
    );
}

bool DSPOverdrive::focus_on_button_short()
{
    if (focus_target == FOCUS_TARGET_PRE_GAIN)
    {
        focus_target = FOCUS_TARGET_POST_GAIN;
        ESP_LOGI(TAG, "Focus target switched to POST");
        return true;
    }
    ESP_LOGI(TAG, "Focus complete on POST -> release requested");
    return false;
}

DSPParameter_t DSPOverdrive::parameter(size_t index) const
{
    switch (index) {
        case 0: return {0, "Input gain", "", pre_gain.load(), 2};
        case 1: return {1, "Output gain", "", post_gain.load(), 2};
        default: return {};
    }
}

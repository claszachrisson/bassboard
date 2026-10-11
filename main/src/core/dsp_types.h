#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

typedef struct DSPParameter
{
    uint16_t id;
    const char* name;
    const char* unit;
    float value;
    uint8_t decimals;
} DSPParameter_t;

typedef enum dsp_block
{
    DSP_BLOCK_OVERDRIVE,
    DSP_BLOCK_DELAY,
    DSP_BLOCK_CHORUS,
    DSP_BLOCK_PHASER,
    DSP_BLOCK_FLANGER,
    DSP_BLOCK_COMPRESSOR,
    DSP_BLOCK_TREMOLO,
} DSPBlock_t;

typedef struct DSPBlockInfo
{
    const char* name;
    const char* name_short;
} DSPBlockInfo_t;

// Metadata strings must have static lifetime. Values are copied, never borrowed.
constexpr size_t DSP_MAX_PARAMETERS = 8;
struct DSPBlockSnapshot {
    DSPBlockInfo_t info = {"Empty", "--"};
    std::array<DSPParameter_t, DSP_MAX_PARAMETERS> parameters{};
    size_t parameter_count = 0;
    int selected_parameter = -1;
    bool present = false;
    bool enabled = false;
    bool focused = false;
};
using DSPBlockChangedCallback = void (*)(int slot, const DSPBlockSnapshot&, void*);


#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace box_audio {
// I2S is configured for two 16-bit slots, not one 32-bit mono sample.
inline void PackStereo(const int16_t* mono, int16_t* stereo, size_t samples, int volume) {
    const int level = std::clamp(volume, 0, 100);
    const int32_t gain = level * level;
    for (size_t i = 0; i < samples; ++i) {
        const auto value = static_cast<int16_t>(int32_t(mono[i]) * gain / 10000);
        stereo[i * 2] = value;
        stereo[i * 2 + 1] = value;
    }
}
}  // namespace box_audio

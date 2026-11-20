#pragma once

#include "BiquadFilter.h"
#include <array>
#include <cstdint>

namespace analog_master::core {

struct CrossoverSplit {
    float low = 0.0f;
    float high = 0.0f;
};

enum class CrossoverSlope : uint8_t {
    LR4_24dB = 0,
    LR8_48dB = 1
};

// Linkwitz-Riley (LR4 24 dB/oct or LR8 48 dB/oct) TPT crossover filter for one audio channel.
// Low-pass and High-pass outputs sum to flat unity magnitude across all frequencies.
class LinkwitzRiley4 {
public:
    LinkwitzRiley4();

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void setCrossoverFrequency(double freqHz) noexcept;
    void setSlope(CrossoverSlope slope) noexcept;

    [[nodiscard]] double crossoverFrequency() const noexcept { return crossoverHz_; }
    [[nodiscard]] CrossoverSlope slope() const noexcept { return slope_; }

    [[nodiscard]] CrossoverSplit process(float sample) noexcept;

private:
    void applyWarpedCutoff(double warpedFreq) noexcept;

    double sampleRate_ = 44100.0;
    double crossoverHz_ = 120.0;
    double targetG_ = 0.0;
    double currentG_ = 0.0;
    double smoothAlpha_ = 1.0;
    CrossoverSlope slope_ = CrossoverSlope::LR4_24dB;
    bool hasProcessed_ = false;

    std::array<BiquadFilter, 4> lpSections_{};
    std::array<BiquadFilter, 4> hpSections_{};
};

} // namespace analog_master::core

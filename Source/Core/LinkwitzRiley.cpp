#include "LinkwitzRiley.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace analog_master::core {

namespace {

constexpr double kMinSampleRate = 8000.0;
constexpr double kDefaultSampleRate = 44100.0;
constexpr double kSmoothingTimeSeconds = 0.0045;
// 4th-order Butterworth damping factors (2 * cos(pi/8) and 2 * cos(3*pi/8)) cascaded twice for LR8
constexpr double kBw4DampA = 1.8477590650225735;
constexpr double kBw4DampB = 0.7653668647301796;

} // namespace

LinkwitzRiley4::LinkwitzRiley4() {
    prepare(kDefaultSampleRate);
}

void LinkwitzRiley4::prepare(double sampleRate) noexcept {
    const double validRate = sampleRate >= kMinSampleRate ? sampleRate : kDefaultSampleRate;
    sampleRate_ = validRate;
    smoothAlpha_ = 1.0 - std::exp(-1.0 / (kSmoothingTimeSeconds * sampleRate_));

    const double safeCutoff = std::clamp(crossoverHz_, 20.0, sampleRate_ * 0.49);
    targetG_ = std::tan(std::numbers::pi * safeCutoff / sampleRate_);
    reset();
}

void LinkwitzRiley4::reset() noexcept {
    for (auto& section : lpSections_) {
        section.reset();
    }
    for (auto& section : hpSections_) {
        section.reset();
    }
    currentG_ = targetG_;
    hasProcessed_ = false;
    applyWarpedCutoff(currentG_);
}

void LinkwitzRiley4::setCrossoverFrequency(double freqHz) noexcept {
    const double safeCutoff = std::clamp(freqHz, 20.0, sampleRate_ * 0.49);
    if (std::abs(crossoverHz_ - safeCutoff) < 1e-4) {
        return;
    }
    crossoverHz_ = safeCutoff;
    targetG_ = std::tan(std::numbers::pi * crossoverHz_ / sampleRate_);
    if (!hasProcessed_) {
        currentG_ = targetG_;
        applyWarpedCutoff(currentG_);
    }
}

void LinkwitzRiley4::setSlope(CrossoverSlope slope) noexcept {
    if (slope_ == slope) {
        return;
    }
    if (slope == CrossoverSlope::LR8_48dB) {
        lpSections_[2].reset();
        lpSections_[3].reset();
        hpSections_[2].reset();
        hpSections_[3].reset();
    }
    slope_ = slope;
    applyWarpedCutoff(currentG_);
}

void LinkwitzRiley4::applyWarpedCutoff(double warpedFreq) noexcept {
    if (slope_ == CrossoverSlope::LR4_24dB) {
        constexpr double kDampLr4 = std::numbers::sqrt2;
        lpSections_[0].setTptLowPass(warpedFreq, kDampLr4);
        lpSections_[1].setTptLowPass(warpedFreq, kDampLr4);
        hpSections_[0].setTptHighPass(warpedFreq, kDampLr4);
        hpSections_[1].setTptHighPass(warpedFreq, kDampLr4);
        return;
    }

    lpSections_[0].setTptLowPass(warpedFreq, kBw4DampA);
    lpSections_[1].setTptLowPass(warpedFreq, kBw4DampB);
    lpSections_[2].setTptLowPass(warpedFreq, kBw4DampA);
    lpSections_[3].setTptLowPass(warpedFreq, kBw4DampB);

    hpSections_[0].setTptHighPass(warpedFreq, kBw4DampA);
    hpSections_[1].setTptHighPass(warpedFreq, kBw4DampB);
    hpSections_[2].setTptHighPass(warpedFreq, kBw4DampA);
    hpSections_[3].setTptHighPass(warpedFreq, kBw4DampB);
}

CrossoverSplit LinkwitzRiley4::process(float sample) noexcept {
    hasProcessed_ = true;

    if (std::abs(targetG_ - currentG_) > 1e-9) {
        currentG_ += smoothAlpha_ * (targetG_ - currentG_);
        if (std::abs(targetG_ - currentG_) <= 1e-9) {
            currentG_ = targetG_;
        }
        applyWarpedCutoff(currentG_);
    }

    float low = lpSections_[1].process(lpSections_[0].process(sample));
    float high = hpSections_[1].process(hpSections_[0].process(sample));
    if (slope_ == CrossoverSlope::LR8_48dB) {
        low = lpSections_[3].process(lpSections_[2].process(low));
        high = hpSections_[3].process(hpSections_[2].process(high));
    }
    return { low, high };
}

} // namespace analog_master::core

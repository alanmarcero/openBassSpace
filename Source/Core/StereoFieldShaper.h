#pragma once

#include "LinkwitzRiley.h"

namespace analog_master::core {

class StereoFieldShaper {
public:
    StereoFieldShaper();

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setCrossoverFrequency(double freqHz) noexcept;
    void setWidth(float width) noexcept; // 0.0f = Mono, 1.0f = Normal, 2.0f = Enhanced
    void setUpperWidth(float width) noexcept;
    void setStereoizeAmount(float amount) noexcept;
    void setSubDrive(float drive) noexcept;
    void setUpperTiltDb(float tiltDb) noexcept;
    void setBypassed(bool bypassed) noexcept;
    void setSoloLow(bool solo) noexcept;

    [[nodiscard]] double crossoverFrequency() const noexcept { return crossoverHz_; }
    [[nodiscard]] float width() const noexcept { return targetWidth_; }
    [[nodiscard]] float upperWidth() const noexcept { return targetUpperWidth_; }
    [[nodiscard]] float stereoizeAmount() const noexcept { return targetStereoize_; }
    [[nodiscard]] float subDrive() const noexcept { return targetSubDrive_; }
    [[nodiscard]] float upperTiltDb() const noexcept { return targetUpperTiltDb_; }
    [[nodiscard]] bool isBypassed() const noexcept { return bypassed_; }
    [[nodiscard]] bool isSoloLow() const noexcept { return soloLow_; }

    [[nodiscard]] static double correlationCoefficient(double cross, double leftEnergy, double rightEnergy) noexcept;

    void processSample(float inL, float inR, float& outL, float& outR) noexcept;
    void processBlock(float* leftChannel, float* rightChannel, int numSamples) noexcept;

private:
    double sampleRate_ = 44100.0;
    double crossoverHz_ = 120.0;
    double currentCrossoverHz_ = 120.0;
    float targetWidth_ = 0.0f; // Default 0.0 = Mono low-end (mastering classic)
    float currentWidth_ = 0.0f;
    float targetUpperWidth_ = 1.0f;
    float currentUpperWidth_ = 1.0f;
    float targetStereoize_ = 0.0f;
    float currentStereoize_ = 0.0f;
    float targetSubDrive_ = 0.0f;
    float currentSubDrive_ = 0.0f;
    float targetUpperTiltDb_ = 0.0f;
    float currentUpperTiltGain_ = 1.0f;
    float allpassInput_ = 0.0f;
    float allpassOutput_ = 0.0f;
    float tiltLpL_ = 0.0f;
    float tiltLpR_ = 0.0f;
    bool bypassed_ = false;
    float currentBypassGain_ = 1.0f; // 1.0 = active, 0.0 = bypassed
    bool soloLow_ = false;
    float currentSoloGain_ = 0.0f; // 1.0 = solo low, 0.0 = full mix
    bool hasProcessedAudio_ = false;

    LinkwitzRiley4 crossoverL_{};
    LinkwitzRiley4 crossoverR_{};
};

} // namespace analog_master::core

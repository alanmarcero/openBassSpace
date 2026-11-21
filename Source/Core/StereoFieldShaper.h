#pragma once

#include "LinkwitzRiley.h"
#include <array>

namespace analog_master::core {

struct StereoCeilingGuard {
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(float& left, float& right) noexcept;

private:
    float limiterGain_ = 1.0f;
    float peakEnv_ = 0.0f;
    float releaseCoeff_ = 0.001f;
    float prev2L_ = 0.0f;
    float prev1L_ = 0.0f;
    float prev2R_ = 0.0f;
    float prev1R_ = 0.0f;
};

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
    void setCrossoverSlope(CrossoverSlope slope) noexcept;
    void setEllipticalMode(bool enabled) noexcept;
    void setBypassed(bool bypassed) noexcept;
    void setSoloLow(bool solo) noexcept;

    [[nodiscard]] double crossoverFrequency() const noexcept { return crossoverHz_; }
    [[nodiscard]] float width() const noexcept { return targetWidth_; }
    [[nodiscard]] float upperWidth() const noexcept { return targetUpperWidth_; }
    [[nodiscard]] float stereoizeAmount() const noexcept { return targetStereoize_; }
    [[nodiscard]] float subDrive() const noexcept { return targetSubDrive_; }
    [[nodiscard]] float upperTiltDb() const noexcept { return targetUpperTiltDb_; }
    [[nodiscard]] CrossoverSlope crossoverSlope() const noexcept { return crossoverSlope_; }
    [[nodiscard]] bool isEllipticalMode() const noexcept { return ellipticalMode_; }
    [[nodiscard]] bool isBypassed() const noexcept { return bypassed_; }
    [[nodiscard]] bool isSoloLow() const noexcept { return soloLow_; }

    [[nodiscard]] static double correlationCoefficient(double cross, double leftEnergy, double rightEnergy) noexcept;

    void processSample(float inL, float inR, float& outL, float& outR) noexcept;
    void processBlock(float* leftChannel, float* rightChannel, int numSamples) noexcept;

private:
    static constexpr size_t kDiffuserStages = 4;
    static constexpr size_t kDiffuserBufferSize = 4800;

    void updateCrossoverDependentFilters(double crossoverHz) noexcept;
    void updateTiltCoefficients(float tiltDb) noexcept;
    [[nodiscard]] float synthesizeDrivenBass(float midLow, float drive) noexcept;
    [[nodiscard]] float processDiffuser(float midHigh) noexcept;
    void processStereoFrame(float dryL, float dryR, float& wetL, float& wetR) noexcept;

    double sampleRate_ = 44100.0;
    double crossoverHz_ = 120.0;
    double currentCrossoverHz_ = 120.0;
    double crossoverSmoothAlpha_ = 1.0;
    double appliedAuxCrossoverHz_ = -1.0;

    float targetWidth_ = 0.0f;
    float currentWidth_ = 0.0f;
    float targetUpperWidth_ = 1.0f;
    float currentUpperWidth_ = 1.0f;
    float targetStereoize_ = 0.0f;
    float currentStereoize_ = 0.0f;
    float targetSubDrive_ = 0.0f;
    float currentSubDrive_ = 0.0f;
    float targetUpperTiltDb_ = 0.0f;
    float currentUpperTiltDb_ = 0.0f;
    float appliedTiltDb_ = -999.0f;
    double tiltPivotG_ = 0.15;

    bool bypassed_ = false;
    float currentBypassGain_ = 1.0f;
    bool soloLow_ = false;
    float currentSoloGain_ = 0.0f;
    CrossoverSlope crossoverSlope_ = CrossoverSlope::LR4_24dB;
    bool ellipticalMode_ = false;
    bool hasProcessedAudio_ = false;

    LinkwitzRiley4 crossoverL_{};
    LinkwitzRiley4 crossoverR_{};

    // Energy-preserving phase-aligned mono sum state
    double powerSmoothCoeff_ = 0.001;
    float makeupSmoothCoeff_ = 0.001f;
    double envMidPower_ = 0.0;
    double envSidePower_ = 0.0;
    float currentMonoMakeup_ = 1.0f;

    // Sub-harmonic synthesizer + ADAA-1 asymmetric saturation + phase-matched post-LPF
    float subTrigHpCoeff_ = 0.003f;
    float subTrigLpCoeff_ = 0.02f;
    float subTrigHpState_ = 0.0f;
    float subTrigLpState_ = 0.0f;
    bool subPositiveHalf_ = false;
    float subFlipFlop_ = 1.0f;
    float subEnv_ = 0.0f;
    float subAttackCoeff_ = 0.01f;
    float subReleaseCoeff_ = 0.001f;
    float prevMidLowSq_ = 0.0f;
    double subAdaaPrevU_ = 0.0;
    double subAdaaPrevAntideriv_ = 0.0;
    BiquadFilter subResonantLpf_{};
    BiquadFilter postDriveLpf_{};
    double phaseMatchG_ = 0.01;
    double lowPhaseApState_ = 0.0;
    double highPhaseApStateL_ = 0.0;
    double highPhaseApStateR_ = 0.0;
    double ellipMidLpG_ = 0.01;
    double ellipMidLpState_ = 0.0;

    // Frequency-progressive 35 Hz Side anchor HPF
    BiquadFilter subSideAnchorHpf_{};

    // Symmetrical TPT upper-band tilt shelves
    BiquadFilter tiltFilterL_{};
    BiquadFilter tiltFilterR_{};

    // Sample-rate-invariant 4-stage Schroeder/Velvet allpass decorrelator
    BiquadFilter stereoizeHpf_{};
    std::array<float, kDiffuserBufferSize> diffuserBuffer_{};
    std::array<int, kDiffuserStages> diffuserOffsets_{};
    std::array<int, kDiffuserStages> diffuserLengths_{};
    std::array<int, kDiffuserStages> diffuserWritePos_{};

    // True-peak-safe M/S output ceiling guard
    StereoCeilingGuard ceilingGuard_{};
};

// Phase correlation over about 300 ms, for the whole band and for the band below the crossover.
// A reading is NaN while its band is below -60 dBFS.
class CorrelationMeter {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void setCrossover(double frequencyHz, CrossoverSlope slope) noexcept;
    void process(float left, float right) noexcept;

    [[nodiscard]] float fullBand() const noexcept { return full_.reading(); }
    [[nodiscard]] float lowBand() const noexcept { return low_.reading(); }

private:
    struct Moments {
        double cross = 0.0;
        double left = 0.0;
        double right = 0.0;
        double power = 0.0;

        void update(float leftSample, float rightSample, double smoothing, double powerSmoothing) noexcept;
        [[nodiscard]] float reading() const noexcept;
    };

    Moments full_;
    Moments low_;
    LinkwitzRiley4 lowLeft_;
    LinkwitzRiley4 lowRight_;
    double smoothing_ = 0.0;
    double powerSmoothing_ = 0.0;
};

} // namespace analog_master::core

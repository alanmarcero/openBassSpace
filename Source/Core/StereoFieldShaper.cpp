#include "StereoFieldShaper.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace analog_master::core {

namespace {

constexpr double kMinSampleRate = 8000.0;
constexpr double kDefaultSampleRate = 44100.0;
constexpr float kCeilingLinear = 0.98855309f; // -0.10 dBFS
constexpr std::array<double, 4> kDiffuserDelaySeconds { 0.0011, 0.0023, 0.0037, 0.0053 };
constexpr std::array<float, 4> kDiffuserGains { 0.62f, -0.58f, 0.55f, -0.52f };
constexpr double kCorrelationSeconds = 0.3;
// A faster follower detects silence within 0.7 s of the audio stopping.
constexpr double kSilenceSeconds = 0.05;
constexpr double kSilentPower = 2.0e-6;

float softCeilingBounded(float x, float ceiling) noexcept {
    const float knee = ceiling * 0.972f;
    const float absX = std::abs(x);
    if (absX <= knee) {
        return x;
    }
    const float span = std::max(1e-5f, ceiling - knee);
    const float shaped = knee + span * std::tanh((absX - knee) / span);
    return x < 0.0f ? -shaped : shaped;
}

float processFirstOrderTptAllpass(float input, double warpedFreq, double& state) noexcept {
    const double x = static_cast<double>(input);
    const double v = (x - state) / (1.0 + warpedFreq);
    const double lowPass = v * warpedFreq + state;
    state = lowPass + v * warpedFreq;
    return static_cast<float>(2.0 * lowPass - x);
}

} // namespace

void StereoCeilingGuard::prepare(double sampleRate) noexcept {
    const double validRate = sampleRate >= kMinSampleRate ? sampleRate : kDefaultSampleRate;
    releaseCoeff_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.030 * validRate)));
    reset();
}

void StereoCeilingGuard::reset() noexcept {
    limiterGain_ = 1.0f;
    peakEnv_ = 0.0f;
    prev2L_ = 0.0f;
    prev1L_ = 0.0f;
    prev2R_ = 0.0f;
    prev1R_ = 0.0f;
}

void StereoCeilingGuard::process(float& left, float& right) noexcept {
    if (!std::isfinite(left) || !std::isfinite(right)) {
        reset();
        left = 0.0f;
        right = 0.0f;
        return;
    }

    // Causal 3-point Lagrange midpoint + slew check captures inter-sample true peaks with zero latency
    const float midInterpL = std::abs(-0.125f * prev2L_ + 0.75f * prev1L_ + 0.375f * left);
    const float midInterpR = std::abs(-0.125f * prev2R_ + 0.75f * prev1R_ + 0.375f * right);
    const float slewMax = std::max(std::abs(left - prev1L_), std::abs(right - prev1R_));
    prev2L_ = prev1L_;
    prev1L_ = left;
    prev2R_ = prev1R_;
    prev1R_ = right;

    const float sampleMax = std::max(std::abs(left), std::abs(right));
    const float truePeakEst = std::max({ sampleMax, midInterpL, midInterpR });
    peakEnv_ = truePeakEst > peakEnv_
        ? truePeakEst
        : peakEnv_ + releaseCoeff_ * (truePeakEst - peakEnv_);

    // Reserve extra true-peak margin when signal is overloaded with high-frequency slew
    float targetCeiling = kCeilingLinear;
    if (peakEnv_ > 1.02f) {
        const float slewRatio = std::clamp(slewMax / (peakEnv_ + 1e-4f), 0.0f, 1.2f);
        targetCeiling = kCeilingLinear / (1.0f + 0.14f * slewRatio);
    }

    const float targetGain = truePeakEst > targetCeiling ? (targetCeiling / truePeakEst) : 1.0f;
    limiterGain_ = targetGain < limiterGain_
        ? targetGain
        : limiterGain_ + releaseCoeff_ * (targetGain - limiterGain_);

    left = softCeilingBounded(left * limiterGain_, kCeilingLinear);
    right = softCeilingBounded(right * limiterGain_, kCeilingLinear);
}

void CorrelationMeter::prepare(double sampleRate) noexcept {
    const double validRate = sampleRate >= kMinSampleRate ? sampleRate : kDefaultSampleRate;
    smoothing_ = 1.0 - std::exp(-1.0 / (kCorrelationSeconds * validRate));
    powerSmoothing_ = 1.0 - std::exp(-1.0 / (kSilenceSeconds * validRate));
    lowLeft_.prepare(validRate);
    lowRight_.prepare(validRate);
    reset();
}

void CorrelationMeter::reset() noexcept {
    full_ = {};
    low_ = {};
    lowLeft_.reset();
    lowRight_.reset();
}

void CorrelationMeter::setCrossover(double frequencyHz, CrossoverSlope slope) noexcept {
    lowLeft_.setCrossoverFrequency(frequencyHz);
    lowRight_.setCrossoverFrequency(frequencyHz);
    lowLeft_.setSlope(slope);
    lowRight_.setSlope(slope);
}

void CorrelationMeter::process(float left, float right) noexcept {
    full_.update(left, right, smoothing_, powerSmoothing_);
    low_.update(lowLeft_.process(left).low, lowRight_.process(right).low, smoothing_, powerSmoothing_);
}

void CorrelationMeter::Moments::update(float leftSample, float rightSample, double smoothing, double powerSmoothing) noexcept {
    const double l = leftSample;
    const double r = rightSample;
    cross += smoothing * (l * r - cross);
    left += smoothing * (l * l - left);
    right += smoothing * (r * r - right);
    power += powerSmoothing * (l * l + r * r - power);
}

float CorrelationMeter::Moments::reading() const noexcept {
    if (power < kSilentPower) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return static_cast<float>(StereoFieldShaper::correlationCoefficient(cross, left, right));
}

StereoFieldShaper::StereoFieldShaper() {
    prepare(kDefaultSampleRate);
}

void StereoFieldShaper::prepare(double sampleRate) noexcept {
    const double validRate = sampleRate >= kMinSampleRate ? sampleRate : kDefaultSampleRate;
    sampleRate_ = validRate;
    currentCrossoverHz_ = crossoverHz_;
    crossoverSmoothAlpha_ = 1.0 - std::exp(-1.0 / (0.0045 * sampleRate_));

    crossoverL_.prepare(sampleRate_);
    crossoverR_.prepare(sampleRate_);
    crossoverL_.setSlope(crossoverSlope_);
    crossoverR_.setSlope(crossoverSlope_);
    crossoverL_.setCrossoverFrequency(currentCrossoverHz_);
    crossoverR_.setCrossoverFrequency(currentCrossoverHz_);

    powerSmoothCoeff_ = 1.0 - std::exp(-1.0 / (0.035 * sampleRate_));
    makeupSmoothCoeff_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.020 * sampleRate_)));

    subTrigHpCoeff_ = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * 24.0 / sampleRate_));
    subAttackCoeff_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.0010 * sampleRate_)));
    subReleaseCoeff_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.0015 * sampleRate_)));

    const double anchorG = std::tan(std::numbers::pi * std::min(35.0, sampleRate_ * 0.45) / sampleRate_);
    subSideAnchorHpf_.setTptHighPass(anchorG, std::numbers::sqrt2);

    const double safeTiltPivot = std::min(2400.0, sampleRate_ * 0.42);
    tiltPivotG_ = std::tan(std::numbers::pi * safeTiltPivot / sampleRate_);

    // Partition packed delay buffer across the 4 sample-rate-invariant Schroeder allpass stages
    int runningOffset = 0;
    for (size_t stage = 0; stage < kDiffuserStages; ++stage) {
        const int rawDelay = static_cast<int>(std::lround(kDiffuserDelaySeconds[stage] * sampleRate_));
        const int maxRemain = static_cast<int>(kDiffuserBufferSize) - runningOffset - static_cast<int>(kDiffuserStages - stage);
        const int delayLen = std::clamp(rawDelay, 1, std::max(1, maxRemain));
        diffuserOffsets_[stage] = runningOffset;
        diffuserLengths_[stage] = delayLen;
        runningOffset += delayLen;
    }

    ceilingGuard_.prepare(sampleRate_);
    reset();
}

void StereoFieldShaper::reset() noexcept {
    crossoverL_.reset();
    crossoverR_.reset();
    currentCrossoverHz_ = crossoverHz_;
    crossoverL_.setSlope(crossoverSlope_);
    crossoverR_.setSlope(crossoverSlope_);
    crossoverL_.setCrossoverFrequency(currentCrossoverHz_);
    crossoverR_.setCrossoverFrequency(currentCrossoverHz_);

    currentWidth_ = targetWidth_;
    currentUpperWidth_ = targetUpperWidth_;
    currentStereoize_ = targetStereoize_;
    currentSubDrive_ = targetSubDrive_;
    currentUpperTiltDb_ = targetUpperTiltDb_;
    currentBypassGain_ = bypassed_ ? 0.0f : 1.0f;
    currentSoloGain_ = soloLow_ ? 1.0f : 0.0f;

    envMidPower_ = 0.0;
    envSidePower_ = 0.0;
    currentMonoMakeup_ = 1.0f;

    subTrigHpState_ = 0.0f;
    subTrigLpState_ = 0.0f;
    subPositiveHalf_ = false;
    subFlipFlop_ = 1.0f;
    subEnv_ = 0.0f;
    prevMidLowSq_ = 0.0f;
    subAdaaPrevU_ = 0.0;
    subAdaaPrevAntideriv_ = 0.0;
    subResonantLpf_.reset();
    postDriveLpf_.reset();
    lowPhaseApState_ = 0.0;
    highPhaseApStateL_ = 0.0;
    highPhaseApStateR_ = 0.0;
    ellipMidLpState_ = 0.0;

    subSideAnchorHpf_.reset();
    tiltFilterL_.reset();
    tiltFilterR_.reset();
    stereoizeHpf_.reset();
    diffuserBuffer_.fill(0.0f);
    diffuserWritePos_.fill(0);
    ceilingGuard_.reset();

    appliedAuxCrossoverHz_ = -1.0;
    appliedTiltDb_ = -999.0f;
    updateCrossoverDependentFilters(currentCrossoverHz_);
    updateTiltCoefficients(currentUpperTiltDb_);

    hasProcessedAudio_ = false;
}

void StereoFieldShaper::updateCrossoverDependentFilters(double crossoverHz) noexcept {
    appliedAuxCrossoverHz_ = crossoverHz;
    const double nyquistLimit = sampleRate_ * 0.45;

    subTrigLpCoeff_ = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * std::clamp(crossoverHz, 35.0, 250.0) / sampleRate_));

    // 2-pole resonant LPF at 0.65 * crossoverHz (Q = 0.90) shapes flip-flop square into round sub-octave
    const double subCutoff = std::clamp(crossoverHz * 0.65, 24.0, nyquistLimit);
    const double subG = std::tan(std::numbers::pi * subCutoff / sampleRate_);
    subResonantLpf_.setTptLowPass(subG, 1.0 / 0.90);

    // Gentle post-saturation harmonic containment LPF + matching 1st-order TPT allpass cutoff
    const double postCutoff = std::clamp(crossoverHz * 1.85, 60.0, nyquistLimit);
    phaseMatchG_ = std::tan(std::numbers::pi * postCutoff / sampleRate_);
    postDriveLpf_.setTptLowPass(phaseMatchG_, std::numbers::sqrt2);

    const double ellipLowCutoff = std::clamp(crossoverHz, 20.0, nyquistLimit);
    ellipMidLpG_ = std::tan(std::numbers::pi * ellipLowCutoff / sampleRate_);

    const double stereoizeHpCutoff = std::clamp(std::max(120.0, crossoverHz), 40.0, nyquistLimit);
    const double stereoizeHpG = std::tan(std::numbers::pi * stereoizeHpCutoff / sampleRate_);
    stereoizeHpf_.setTptHighPass(stereoizeHpG, std::numbers::sqrt2);
}

void StereoFieldShaper::updateTiltCoefficients(float tiltDb) noexcept {
    appliedTiltDb_ = tiltDb;
    tiltFilterL_.setTptHighShelf(tiltPivotG_, static_cast<double>(tiltDb));
    tiltFilterR_.setTptHighShelf(tiltPivotG_, static_cast<double>(tiltDb));
}

void StereoFieldShaper::setCrossoverFrequency(double freqHz) noexcept {
    const double clamped = std::clamp(freqHz, 20.0, 500.0);
    if (std::abs(crossoverHz_ - clamped) < 0.001) {
        return;
    }
    crossoverHz_ = clamped;
    crossoverL_.setCrossoverFrequency(crossoverHz_);
    crossoverR_.setCrossoverFrequency(crossoverHz_);
    if (!hasProcessedAudio_) {
        currentCrossoverHz_ = crossoverHz_;
        updateCrossoverDependentFilters(currentCrossoverHz_);
    }
}

void StereoFieldShaper::setWidth(float width) noexcept {
    targetWidth_ = std::clamp(width, 0.0f, 2.0f);
    if (!hasProcessedAudio_) {
        currentWidth_ = targetWidth_;
    }
}

void StereoFieldShaper::setUpperWidth(float width) noexcept {
    targetUpperWidth_ = std::clamp(width, 0.0f, 2.0f);
    if (!hasProcessedAudio_) {
        currentUpperWidth_ = targetUpperWidth_;
    }
}

void StereoFieldShaper::setStereoizeAmount(float amount) noexcept {
    targetStereoize_ = std::clamp(amount, 0.0f, 1.0f);
    if (!hasProcessedAudio_) {
        currentStereoize_ = targetStereoize_;
    }
}

void StereoFieldShaper::setSubDrive(float drive) noexcept {
    targetSubDrive_ = std::clamp(drive, -1.0f, 1.0f);
    if (!hasProcessedAudio_) {
        currentSubDrive_ = targetSubDrive_;
    }
}

void StereoFieldShaper::setUpperTiltDb(float tiltDb) noexcept {
    targetUpperTiltDb_ = std::clamp(tiltDb, -6.0f, 6.0f);
    if (!hasProcessedAudio_) {
        currentUpperTiltDb_ = targetUpperTiltDb_;
        updateTiltCoefficients(currentUpperTiltDb_);
    }
}

void StereoFieldShaper::setCrossoverSlope(CrossoverSlope slope) noexcept {
    if (crossoverSlope_ == slope) {
        return;
    }
    crossoverSlope_ = slope;
    crossoverL_.setSlope(slope);
    crossoverR_.setSlope(slope);
}

void StereoFieldShaper::setEllipticalMode(bool enabled) noexcept {
    if (ellipticalMode_ == enabled) {
        return;
    }
    ellipticalMode_ = enabled;
    crossoverL_.reset();
    crossoverR_.reset();
    ellipMidLpState_ = 0.0;
}

double StereoFieldShaper::correlationCoefficient(double cross, double leftEnergy, double rightEnergy) noexcept {
    if (!std::isfinite(cross) || !std::isfinite(leftEnergy) || !std::isfinite(rightEnergy)
        || leftEnergy <= 1e-20 || rightEnergy <= 1e-20) {
        return 0.0;
    }
    return std::clamp(cross / std::sqrt(leftEnergy * rightEnergy), -1.0, 1.0);
}

void StereoFieldShaper::setBypassed(bool bypassed) noexcept {
    bypassed_ = bypassed;
    if (!hasProcessedAudio_) {
        currentBypassGain_ = bypassed_ ? 0.0f : 1.0f;
    }
}

void StereoFieldShaper::setSoloLow(bool solo) noexcept {
    soloLow_ = solo;
    if (!hasProcessedAudio_) {
        currentSoloGain_ = soloLow_ ? 1.0f : 0.0f;
    }
}

float StereoFieldShaper::synthesizeDrivenBass(float midLow, float drive) noexcept {
    // Band-limit trigger signal so DC or harmonics never cause false octave-divider flips
    subTrigHpState_ += subTrigHpCoeff_ * (midLow - subTrigHpState_);
    const float dcFreeLow = midLow - subTrigHpState_;
    subTrigLpState_ += subTrigLpCoeff_ * (dcFreeLow - subTrigLpState_);

    constexpr float kZeroCrossHysteresis = 1e-5f;
    if (!subPositiveHalf_ && subTrigLpState_ > kZeroCrossHysteresis) {
        subPositiveHalf_ = true;
        subFlipFlop_ = -subFlipFlop_;
    }
    if (subPositiveHalf_ && subTrigLpState_ < -kZeroCrossHysteresis) {
        subPositiveHalf_ = false;
    }

    const float absLow = std::abs(midLow);
    const float envCoeff = absLow > subEnv_ ? subAttackCoeff_ : subReleaseCoeff_;
    subEnv_ += envCoeff * (absLow - subEnv_);

    // Mu-Tron style rectified octave divider: zero DC in steady state and tight envelope gate on note-off
    const float subAmplitude = 0.85f * absLow + 0.15f * subEnv_;
    const float subGate = std::clamp(subEnv_ * 15.0f, 0.0f, 1.0f);
    const float subOctave = subResonantLpf_.process(subFlipFlop_ * subAmplitude) * subGate;

    // Exact ADAA-1 tanh for odd harmonics + telescoping quadratic difference for zero-DC 2f tube warmth
    const double driveGain = 1.0 + 2.5 * static_cast<double>(drive);
    const double u = static_cast<double>(midLow) * driveGain;
    const double absU = std::abs(u);
    const double currAntideriv = absU + std::log1p(std::exp(-2.0 * absU));
    const double diffU = u - subAdaaPrevU_;

    double satRaw = 0.0;
    if (std::abs(diffU) > 1e-5) {
        satRaw = (currAntideriv - subAdaaPrevAntideriv_) / diffU;
    }
    if (std::abs(diffU) <= 1e-5) {
        satRaw = std::tanh(0.5 * (u + subAdaaPrevU_));
    }

    // Telescoping difference (x_prev^2 - x^2) scaled by sampleRate generates pure 2f warmth with zero DC
    const float sqCurr = midLow * midLow;
    const float srNorm = static_cast<float>(sampleRate_ / 48000.0);
    const float evenHarmonic = (prevMidLowSq_ - sqCurr) * (7.5f * srNorm * drive);
    prevMidLowSq_ = sqCurr;

    subAdaaPrevU_ = u;
    subAdaaPrevAntideriv_ = currAntideriv;

    const float satClean = static_cast<float>(satRaw / (1.0 + 1.15 * static_cast<double>(drive))) + evenHarmonic;
    const float excitedLow = satClean + (0.65f * drive) * subOctave;
    return postDriveLpf_.process(excitedLow);
}

float StereoFieldShaper::processDiffuser(float midHigh) noexcept {
    float signal = stereoizeHpf_.process(midHigh);
    for (size_t stage = 0; stage < kDiffuserStages; ++stage) {
        const int base = diffuserOffsets_[stage];
        const int length = diffuserLengths_[stage];
        const int pos = diffuserWritePos_[stage];
        const float coeff = kDiffuserGains[stage];

        const float delayed = diffuserBuffer_[static_cast<size_t>(base + pos)];
        const float state = signal - coeff * delayed;
        diffuserBuffer_[static_cast<size_t>(base + pos)] = state;
        signal = delayed + coeff * state;

        const int nextPos = pos + 1;
        diffuserWritePos_[stage] = nextPos >= length ? 0 : nextPos;
    }
    return signal;
}

void StereoFieldShaper::processStereoFrame(float dryL, float dryR, float& wetL, float& wetR) noexcept {
    if (std::abs(crossoverHz_ - currentCrossoverHz_) > 1e-4) {
        currentCrossoverHz_ += crossoverSmoothAlpha_ * (crossoverHz_ - currentCrossoverHz_);
        if (std::abs(crossoverHz_ - currentCrossoverHz_) <= 1e-4) {
            currentCrossoverHz_ = crossoverHz_;
        }
    }
    if (std::abs(currentCrossoverHz_ - appliedAuxCrossoverHz_) > 0.05) {
        updateCrossoverDependentFilters(currentCrossoverHz_);
    }
    if (std::abs(currentUpperTiltDb_ - appliedTiltDb_) > 1e-4f) {
        updateTiltCoefficients(currentUpperTiltDb_);
    }

    // One bipolar knob: positive adds sub drive, negative turns the bass band down toward silence.
    const float subDrive = std::max(currentSubDrive_, 0.0f);
    const float bassGain = 1.0f + std::min(currentSubDrive_, 0.0f);

    const float highMix = 1.0f - currentSoloGain_;
    const float widenBlend = std::clamp(currentWidth_ - 1.0f, 0.0f, 1.0f);

    const float rawMid = 0.5f * (dryL + dryR);
    const float rawSide = 0.5f * (dryL - dryR);
    const float xoverInA = ellipticalMode_ ? rawMid : dryL;
    const float xoverInB = ellipticalMode_ ? rawSide : dryR;
    const auto splitA = crossoverL_.process(xoverInA);
    const auto splitB = crossoverR_.process(xoverInB);

    const float rawMidLow = ellipticalMode_ ? splitA.low : 0.5f * (splitA.low + splitB.low);
    const float rawSideLow = ellipticalMode_ ? splitB.low : 0.5f * (splitA.low - splitB.low);

    // Energy-preserving low-band mono makeup recovers -3..-6 dB phase-offset bass dropouts
    const double midPower = static_cast<double>(rawMidLow) * rawMidLow;
    const double sidePower = static_cast<double>(rawSideLow) * rawSideLow;
    envMidPower_ += powerSmoothCoeff_ * (midPower - envMidPower_);
    envSidePower_ += powerSmoothCoeff_ * (sidePower - envSidePower_);

    const double narrowAmount = std::max(0.0, 1.0 - static_cast<double>(currentWidth_) * currentWidth_);
    double targetMakeup = 1.0;
    if (narrowAmount > 1e-5 && envMidPower_ > 1e-8 && envMidPower_ > 0.05 * (envMidPower_ + envSidePower_)) {
        targetMakeup = std::clamp(std::sqrt(1.0 + (narrowAmount * envSidePower_) / (envMidPower_ + 1e-9)), 1.0, 2.0);
    }
    currentMonoMakeup_ += makeupSmoothCoeff_ * (static_cast<float>(targetMakeup) - currentMonoMakeup_);

    float finalMid = 0.0f;
    float tiltedSideHigh = 0.0f;
    float midForDiffuser = 0.0f;

    if (ellipticalMode_) {
        // Zero-phase-sum 1st-order low extraction keeps Mid 100% phase-linear when subDrive == 0
        const double vEllip = (static_cast<double>(rawMid) - ellipMidLpState_) / (1.0 + ellipMidLpG_);
        const float ellipMidLow = static_cast<float>(vEllip * ellipMidLpG_ + ellipMidLpState_);
        ellipMidLpState_ = static_cast<double>(ellipMidLow) + vEllip * ellipMidLpG_;

        const float drivenLow = synthesizeDrivenBass(ellipMidLow, subDrive);
        // Cutting bass crossfades the Mid toward its crossover high band, since the 1st-order extraction is too gentle to remove bass.
        float shapedMid = rawMid + subDrive * (drivenLow - ellipMidLow) + (1.0f - bassGain) * (splitA.high - rawMid);
        if (std::abs(currentUpperTiltDb_) > 1e-5f) {
            shapedMid = tiltFilterL_.process(shapedMid);
        }

        const float soloMid = bassGain * (splitA.low * currentMonoMakeup_ + subDrive * (drivenLow - splitA.low));
        finalMid = shapedMid * highMix + soloMid * currentSoloGain_;

        tiltedSideHigh = splitB.high;
        if (std::abs(currentUpperTiltDb_) > 1e-5f) {
            tiltedSideHigh = tiltFilterR_.process(splitB.high);
        }
        midForDiffuser = finalMid;
    }

    if (!ellipticalMode_) {
        const float makeupMidLow = rawMidLow * currentMonoMakeup_;
        const float drivenLow = synthesizeDrivenBass(makeupMidLow, subDrive);
        const float phaseMatchedDryLow = processFirstOrderTptAllpass(makeupMidLow, phaseMatchG_, lowPhaseApState_);
        const float phaseMatchedHighL = processFirstOrderTptAllpass(splitA.high, phaseMatchG_, highPhaseApStateL_);
        const float phaseMatchedHighR = processFirstOrderTptAllpass(splitB.high, phaseMatchG_, highPhaseApStateR_);

        const float phaseAlignMix = std::clamp(subDrive * 4.0f, 0.0f, 1.0f);
        const float blendedDrivenLow = phaseMatchedDryLow + subDrive * (drivenLow - phaseMatchedDryLow);
        const float midLow = bassGain * (makeupMidLow + phaseAlignMix * (blendedDrivenLow - makeupMidLow));

        const float alignedHighL = splitA.high + phaseAlignMix * (phaseMatchedHighL - splitA.high);
        const float alignedHighR = splitB.high + phaseAlignMix * (phaseMatchedHighR - splitB.high);

        const float tiltedHighL = tiltFilterL_.process(alignedHighL);
        const float tiltedHighR = tiltFilterR_.process(alignedHighR);
        const float midHigh = 0.5f * (tiltedHighL + tiltedHighR);

        finalMid = midLow + highMix * midHigh;
        tiltedSideHigh = 0.5f * (tiltedHighL - tiltedHighR);
        midForDiffuser = midHigh;
    }

    // Frequency-progressive 35 Hz Side HPF centers sub-40 Hz rumble when bass width > 100%
    const float anchoredSideLow = subSideAnchorHpf_.process(rawSideLow);
    const float progressiveSideLow = rawSideLow + widenBlend * (anchoredSideLow - rawSideLow);
    const float baseSide = bassGain * progressiveSideLow * currentWidth_ + highMix * (tiltedSideHigh * currentUpperWidth_);

    wetL = finalMid + baseSide;
    wetR = finalMid - baseSide;
    ceilingGuard_.process(wetL, wetR);

    const float diffusedHigh = processDiffuser(midForDiffuser);
    const float stereoizeHeadroom = 0.70f * std::max(0.0f, 0.935f - std::max(std::abs(wetL), std::abs(wetR)));
    const float rawStereoize = highMix * diffusedHigh * (0.55f * currentStereoize_);
    float stereoizedSide = 0.0f;
    if (stereoizeHeadroom > 1e-6f) {
        stereoizedSide = softCeilingBounded(rawStereoize, stereoizeHeadroom);
    }
    wetL += stereoizedSide;
    wetR -= stereoizedSide;
}

void StereoFieldShaper::processSample(float inL, float inR, float& outL, float& outR) noexcept {
    hasProcessedAudio_ = true;
    if (bypassed_) {
        outL = inL;
        outR = inR;
        return;
    }

    currentWidth_ = targetWidth_;
    currentUpperWidth_ = targetUpperWidth_;
    currentStereoize_ = targetStereoize_;
    currentSubDrive_ = targetSubDrive_;
    currentUpperTiltDb_ = targetUpperTiltDb_;
    currentSoloGain_ = soloLow_ ? 1.0f : 0.0f;

    processStereoFrame(inL, inR, outL, outR);
}

void StereoFieldShaper::processBlock(float* leftChannel, float* rightChannel, int numSamples) noexcept {
    if (leftChannel == nullptr || numSamples <= 0) {
        return;
    }
    hasProcessedAudio_ = true;

    const float targetBypassGain = bypassed_ ? 0.0f : 1.0f;
    if (bypassed_ && currentBypassGain_ <= 1e-5f) {
        currentBypassGain_ = 0.0f;
        currentWidth_ = targetWidth_;
        currentUpperWidth_ = targetUpperWidth_;
        currentStereoize_ = targetStereoize_;
        currentSubDrive_ = targetSubDrive_;
        currentUpperTiltDb_ = targetUpperTiltDb_;
        currentCrossoverHz_ = crossoverHz_;
        return;
    }

    const float invSamples = 1.0f / static_cast<float>(numSamples);
    const float widthStep = (targetWidth_ - currentWidth_) * invSamples;
    const float upperWidthStep = (targetUpperWidth_ - currentUpperWidth_) * invSamples;
    const float stereoizeStep = (targetStereoize_ - currentStereoize_) * invSamples;
    const float subDriveStep = (targetSubDrive_ - currentSubDrive_) * invSamples;
    const float tiltDbStep = (targetUpperTiltDb_ - currentUpperTiltDb_) * invSamples;
    const float targetSoloGain = soloLow_ ? 1.0f : 0.0f;
    const float soloStep = (targetSoloGain - currentSoloGain_) * invSamples;
    const float bypassStep = (targetBypassGain - currentBypassGain_) * invSamples;

    if (rightChannel == nullptr) {
        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
            currentWidth_ += widthStep;
            currentUpperWidth_ += upperWidthStep;
            currentStereoize_ += stereoizeStep;
            currentSubDrive_ += subDriveStep;
            currentUpperTiltDb_ += tiltDbStep;
            currentSoloGain_ += soloStep;
            currentBypassGain_ += bypassStep;

            const float dry = leftChannel[sampleIndex];
            float wetL = 0.0f;
            float wetR = 0.0f;
            processStereoFrame(dry, dry, wetL, wetR);
            const float monoWet = 0.5f * (wetL + wetR);
            leftChannel[sampleIndex] = dry + currentBypassGain_ * (monoWet - dry);
        }
        currentWidth_ = targetWidth_;
        currentUpperWidth_ = targetUpperWidth_;
        currentStereoize_ = targetStereoize_;
        currentSubDrive_ = targetSubDrive_;
        currentUpperTiltDb_ = targetUpperTiltDb_;
        currentSoloGain_ = targetSoloGain;
        currentBypassGain_ = targetBypassGain;
        return;
    }

    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
        currentWidth_ += widthStep;
        currentUpperWidth_ += upperWidthStep;
        currentStereoize_ += stereoizeStep;
        currentSubDrive_ += subDriveStep;
        currentUpperTiltDb_ += tiltDbStep;
        currentSoloGain_ += soloStep;
        currentBypassGain_ += bypassStep;

        const float dryL = leftChannel[sampleIndex];
        const float dryR = rightChannel[sampleIndex];
        float wetL = 0.0f;
        float wetR = 0.0f;
        processStereoFrame(dryL, dryR, wetL, wetR);

        leftChannel[sampleIndex] = dryL + currentBypassGain_ * (wetL - dryL);
        rightChannel[sampleIndex] = dryR + currentBypassGain_ * (wetR - dryR);
    }

    currentWidth_ = targetWidth_;
    currentUpperWidth_ = targetUpperWidth_;
    currentStereoize_ = targetStereoize_;
    currentSubDrive_ = targetSubDrive_;
    currentUpperTiltDb_ = targetUpperTiltDb_;
    currentSoloGain_ = targetSoloGain;
    currentBypassGain_ = targetBypassGain;
}

} // namespace analog_master::core

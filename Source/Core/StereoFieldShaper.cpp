#include "StereoFieldShaper.h"
#include <algorithm>
#include <cmath>

namespace analog_master::core {

StereoFieldShaper::StereoFieldShaper() {
    prepare(44100.0);
}

void StereoFieldShaper::prepare(double sampleRate) noexcept {
    if (sampleRate < 8000.0) {
        sampleRate = 44100.0;
    }
    sampleRate_ = sampleRate;
    currentCrossoverHz_ = crossoverHz_;
    crossoverL_.prepare(sampleRate);
    crossoverR_.prepare(sampleRate);
    crossoverL_.setCrossoverFrequency(currentCrossoverHz_);
    crossoverR_.setCrossoverFrequency(currentCrossoverHz_);
    currentBypassGain_ = bypassed_ ? 0.0f : 1.0f;
    currentSoloGain_ = soloLow_ ? 1.0f : 0.0f;
    hasProcessedAudio_ = false;
    reset();
}

void StereoFieldShaper::reset() noexcept {
    crossoverL_.reset();
    crossoverR_.reset();
    currentCrossoverHz_ = crossoverHz_;
    crossoverL_.setCrossoverFrequency(currentCrossoverHz_);
    crossoverR_.setCrossoverFrequency(currentCrossoverHz_);
    currentWidth_ = targetWidth_;
    currentUpperWidth_ = targetUpperWidth_;
    currentStereoize_ = targetStereoize_;
    currentSubDrive_ = targetSubDrive_;
    currentUpperTiltGain_ = std::pow(10.0f, targetUpperTiltDb_ / 20.0f);
    allpassInput_ = 0.0f;
    allpassOutput_ = 0.0f;
    tiltLpL_ = 0.0f;
    tiltLpR_ = 0.0f;
    currentBypassGain_ = bypassed_ ? 0.0f : 1.0f;
    currentSoloGain_ = soloLow_ ? 1.0f : 0.0f;
    hasProcessedAudio_ = false;
}

void StereoFieldShaper::setCrossoverFrequency(double freqHz) noexcept {
    const double clamped = std::clamp(freqHz, 20.0, 500.0);
    if (std::abs(crossoverHz_ - clamped) < 0.001) {
        return;
    }
    crossoverHz_ = clamped;
    if (!hasProcessedAudio_) {
        currentCrossoverHz_ = crossoverHz_;
        crossoverL_.setCrossoverFrequency(currentCrossoverHz_);
        crossoverR_.setCrossoverFrequency(currentCrossoverHz_);
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
    targetSubDrive_ = std::clamp(drive, 0.0f, 1.0f);
    if (!hasProcessedAudio_) {
        currentSubDrive_ = targetSubDrive_;
    }
}

void StereoFieldShaper::setUpperTiltDb(float tiltDb) noexcept {
    targetUpperTiltDb_ = std::clamp(tiltDb, -6.0f, 6.0f);
    if (!hasProcessedAudio_) {
        currentUpperTiltGain_ = std::pow(10.0f, targetUpperTiltDb_ / 20.0f);
    }
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

void StereoFieldShaper::processSample(float inL, float inR, float& outL, float& outR) noexcept {
    hasProcessedAudio_ = true;
    if (bypassed_) {
        outL = inL;
        outR = inR;
        return;
    }

    if (std::abs(currentCrossoverHz_ - crossoverHz_) > 1e-3) {
        currentCrossoverHz_ = crossoverHz_;
        crossoverL_.setCrossoverFrequency(currentCrossoverHz_);
        crossoverR_.setCrossoverFrequency(currentCrossoverHz_);
    }

    const auto splitL = crossoverL_.process(inL);
    const auto splitR = crossoverR_.process(inR);

    // Mid/Side matrix of low frequencies (< crossover)
    float midLow = 0.5f * (splitL.low + splitR.low);
    if (targetSubDrive_ > 1e-4f) {
        const float driveGain = 1.0f + 2.5f * targetSubDrive_;
        const float saturated = std::tanh(midLow * driveGain) / (1.0f + 1.15f * targetSubDrive_);
        midLow += (saturated - midLow) * targetSubDrive_;
    }
    const float sideLow = 0.5f * (splitL.low - splitR.low);

    const float shapedSideLow = sideLow * targetWidth_;

    const float shapedLowL = midLow + shapedSideLow;
    const float shapedLowR = midLow - shapedSideLow;

    if (soloLow_) {
        outL = shapedLowL;
        outR = shapedLowR;
        return;
    }

    const float tiltAlpha = static_cast<float>(1.0 - std::exp(-2.0 * 3.141592653589793 * 2400.0 / sampleRate_));
    tiltLpL_ += tiltAlpha * (splitL.high - tiltLpL_);
    tiltLpR_ += tiltAlpha * (splitR.high - tiltLpR_);
    const float targetTiltGain = std::pow(10.0f, targetUpperTiltDb_ / 20.0f);
    const float tiltedHighL = tiltLpL_ + (splitL.high - tiltLpL_) * targetTiltGain;
    const float tiltedHighR = tiltLpR_ + (splitR.high - tiltLpR_) * targetTiltGain;

    const float midHigh = 0.5f * (tiltedHighL + tiltedHighR);
    const float sideHigh = 0.5f * (tiltedHighL - tiltedHighR) * targetUpperWidth_;
    constexpr float allpassCoefficient = 0.7f;
    const float decorrelatedHigh = allpassCoefficient * midHigh + allpassInput_ - allpassCoefficient * allpassOutput_;
    allpassInput_ = midHigh;
    allpassOutput_ = decorrelatedHigh;
    const float stereoizedSide = (decorrelatedHigh - midHigh) * targetStereoize_;
    outL = shapedLowL + midHigh + sideHigh + stereoizedSide;
    outR = shapedLowR + midHigh - sideHigh - stereoizedSide;
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
        currentCrossoverHz_ = crossoverHz_;
        return;
    }

    const float startWidth = currentWidth_;
    const float endWidth = targetWidth_;
    const float widthStep = (endWidth - startWidth) / static_cast<float>(numSamples);
    const float upperWidthStep = (targetUpperWidth_ - currentUpperWidth_) / static_cast<float>(numSamples);
    const float stereoizeStep = (targetStereoize_ - currentStereoize_) / static_cast<float>(numSamples);
    const float subDriveStep = (targetSubDrive_ - currentSubDrive_) / static_cast<float>(numSamples);
    const float targetTiltGain = std::pow(10.0f, targetUpperTiltDb_ / 20.0f);
    const float tiltGainStep = (targetTiltGain - currentUpperTiltGain_) / static_cast<float>(numSamples);
    const float tiltAlpha = static_cast<float>(1.0 - std::exp(-2.0 * 3.141592653589793 * 2400.0 / (sampleRate_ > 1000.0 ? sampleRate_ : 44100.0)));

    const float targetSoloGain = soloLow_ ? 1.0f : 0.0f;
    const float soloStep = (targetSoloGain - currentSoloGain_) / static_cast<float>(numSamples);
    const float bypassStep = (targetBypassGain - currentBypassGain_) / static_cast<float>(numSamples);

    if (rightChannel == nullptr) {
        // Mono channel processing
        if ( std::abs(currentCrossoverHz_ - crossoverHz_) > 1e-3 ) {
            currentCrossoverHz_ = crossoverHz_;
            crossoverL_.setCrossoverFrequency(currentCrossoverHz_);
        }
        for (int i = 0; i < numSamples; ++i) {
            currentSoloGain_ += soloStep;
            currentBypassGain_ += bypassStep;
            currentUpperWidth_ += upperWidthStep;
            currentStereoize_ += stereoizeStep;
            currentSubDrive_ += subDriveStep;
            currentUpperTiltGain_ += tiltGainStep;
            const float dry = leftChannel[i];
            const auto split = crossoverL_.process(dry);
            float lowVal = split.low;
            if (currentSubDrive_ > 1e-4f) {
                const float driveGain = 1.0f + 2.5f * currentSubDrive_;
                const float sat = std::tanh(lowVal * driveGain) / (1.0f + 1.15f * currentSubDrive_);
                lowVal += (sat - lowVal) * currentSubDrive_;
            }
            tiltLpL_ += tiltAlpha * (split.high - tiltLpL_);
            const float highVal = tiltLpL_ + (split.high - tiltLpL_) * currentUpperTiltGain_;
            const float wet = lowVal + (1.0f - currentSoloGain_) * highVal;
            leftChannel[i] = dry + currentBypassGain_ * (wet - dry);
        }
        currentWidth_ = endWidth;
        currentUpperWidth_ = targetUpperWidth_;
        currentStereoize_ = targetStereoize_;
        currentSubDrive_ = targetSubDrive_;
        currentUpperTiltGain_ = targetTiltGain;
        currentSoloGain_ = targetSoloGain;
        currentBypassGain_ = targetBypassGain;
        return;
    }

    // Stereo channel processing with sub-block crossover smoothing & click-free width/solo/bypass interpolation
    constexpr int kSubBlockSize = 16;
    const double effectiveSr = sampleRate_ > 1000.0 ? sampleRate_ : 44100.0;
    const double smoothingCoeff = 1.0 - std::exp(-static_cast<double>(kSubBlockSize) / (0.006 * effectiveSr));

    int sampleIdx = 0;
    while (sampleIdx < numSamples) {
        const int chunk = std::min(kSubBlockSize, numSamples - sampleIdx);

        if (std::abs(currentCrossoverHz_ - crossoverHz_) > 1e-3) {
            const double logCurr = std::log(std::max(20.0, currentCrossoverHz_));
            const double logTarget = std::log(std::max(20.0, crossoverHz_));
            currentCrossoverHz_ = std::exp(logCurr + smoothingCoeff * (logTarget - logCurr));
            if (std::abs(currentCrossoverHz_ - crossoverHz_) < 0.02) {
                currentCrossoverHz_ = crossoverHz_;
            }
            crossoverL_.setCrossoverFrequency(currentCrossoverHz_);
            crossoverR_.setCrossoverFrequency(currentCrossoverHz_);
        }

        for (int i = sampleIdx; i < sampleIdx + chunk; ++i) {
            currentWidth_ += widthStep;
            currentUpperWidth_ += upperWidthStep;
            currentStereoize_ += stereoizeStep;
            currentSubDrive_ += subDriveStep;
            currentUpperTiltGain_ += tiltGainStep;
            currentSoloGain_ += soloStep;
            currentBypassGain_ += bypassStep;

            const float dryL = leftChannel[i];
            const float dryR = rightChannel[i];

            const auto splitL = crossoverL_.process(dryL);
            const auto splitR = crossoverR_.process(dryR);

            float midLow = 0.5f * (splitL.low + splitR.low);
            if (currentSubDrive_ > 1e-4f) {
                const float driveGain = 1.0f + 2.5f * currentSubDrive_;
                const float sat = std::tanh(midLow * driveGain) / (1.0f + 1.15f * currentSubDrive_);
                midLow += (sat - midLow) * currentSubDrive_;
            }
            const float sideLow = 0.5f * (splitL.low - splitR.low);

            const float shapedSideLow = sideLow * currentWidth_;

            const float shapedLowL = midLow + shapedSideLow;
            const float shapedLowR = midLow - shapedSideLow;

            tiltLpL_ += tiltAlpha * (splitL.high - tiltLpL_);
            tiltLpR_ += tiltAlpha * (splitR.high - tiltLpR_);
            const float tiltedHighL = tiltLpL_ + (splitL.high - tiltLpL_) * currentUpperTiltGain_;
            const float tiltedHighR = tiltLpR_ + (splitR.high - tiltLpR_) * currentUpperTiltGain_;

            const float midHigh = 0.5f * (tiltedHighL + tiltedHighR);
            const float sideHigh = 0.5f * (tiltedHighL - tiltedHighR) * currentUpperWidth_;
            constexpr float allpassCoefficient = 0.7f;
            const float decorrelatedHigh = allpassCoefficient * midHigh + allpassInput_ - allpassCoefficient * allpassOutput_;
            allpassInput_ = midHigh;
            allpassOutput_ = decorrelatedHigh;
            const float stereoizedSide = (decorrelatedHigh - midHigh) * currentStereoize_;

            const float highMix = 1.0f - currentSoloGain_;
            const float wetL = shapedLowL + highMix * (midHigh + sideHigh + stereoizedSide);
            const float wetR = shapedLowR + highMix * (midHigh - sideHigh - stereoizedSide);

            leftChannel[i] = dryL + currentBypassGain_ * (wetL - dryL);
            rightChannel[i] = dryR + currentBypassGain_ * (wetR - dryR);
        }

        sampleIdx += chunk;
    }

    currentWidth_ = endWidth;
    currentUpperWidth_ = targetUpperWidth_;
    currentStereoize_ = targetStereoize_;
    currentSubDrive_ = targetSubDrive_;
    currentUpperTiltGain_ = targetTiltGain;
    currentSoloGain_ = targetSoloGain;
    currentBypassGain_ = targetBypassGain;
}

} // namespace analog_master::core

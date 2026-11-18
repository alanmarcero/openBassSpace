#pragma once

#include "ArtworkBeatClock.h"
#include "Parameters.h"
#include "StereoFieldShaper.h"
#include <juce_audio_processors/juce_audio_processors.h>

class BassSpaceProcessor final : public juce::AudioProcessor {
public:
    BassSpaceProcessor();
    ~BassSpaceProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destination) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    [[nodiscard]] float currentFrequency() const noexcept {
        return freqParam_ != nullptr ? freqParam_->load(std::memory_order_relaxed) : 120.0f;
    }

    [[nodiscard]] float currentWidth() const noexcept {
        return widthParam_ != nullptr ? widthParam_->load(std::memory_order_relaxed) : 0.0f;
    }

    [[nodiscard]] float currentUpperWidth() const noexcept {
        return upperWidthParam_ != nullptr ? upperWidthParam_->load(std::memory_order_relaxed) : 100.0f;
    }

    [[nodiscard]] float currentStereoize() const noexcept {
        return stereoizeParam_ != nullptr ? stereoizeParam_->load(std::memory_order_relaxed) : 0.0f;
    }

    [[nodiscard]] float currentSubDrive() const noexcept {
        return subDriveParam_ != nullptr ? subDriveParam_->load(std::memory_order_relaxed) : 0.0f;
    }

    [[nodiscard]] float currentUpperTilt() const noexcept {
        return upperTiltParam_ != nullptr ? upperTiltParam_->load(std::memory_order_relaxed) : 0.0f;
    }

    [[nodiscard]] bool currentSoloLow() const noexcept {
        return soloLowParam_ != nullptr && soloLowParam_->load(std::memory_order_relaxed) > 0.5f;
    }

    [[nodiscard]] float currentCorrelation() const noexcept {
        return correlation_.load(std::memory_order_relaxed);
    }

    float artworkPhase() const noexcept { return artworkClock_.currentPhase(); }

    juce::AudioProcessorValueTreeState parameters;

private:
    std::atomic<float>* freqParam_ = nullptr;
    std::atomic<float>* widthParam_ = nullptr;
    std::atomic<float>* upperWidthParam_ = nullptr;
    std::atomic<float>* stereoizeParam_ = nullptr;
    std::atomic<float>* outputDbParam_ = nullptr;
    std::atomic<float>* subDriveParam_ = nullptr;
    std::atomic<float>* upperTiltParam_ = nullptr;
    std::atomic<float>* soloLowParam_ = nullptr;
    ArtworkBeatClock artworkClock_;
    double processingRate_ = 48000.0;
    std::atomic<float> correlation_ {0.0f};
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain_;
    analog_master::core::StereoFieldShaper shaper_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassSpaceProcessor)
};

#pragma once

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

    juce::AudioProcessorValueTreeState parameters;

private:
    std::atomic<float>* freqParam_ = nullptr;
    std::atomic<float>* widthParam_ = nullptr;
    analog_master::core::StereoFieldShaper shaper_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassSpaceProcessor)
};

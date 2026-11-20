#pragma once

#include "ArtworkBeatClock.h"
#include "Parameters.h"
#include "PresetLibrary.h"
#include "StereoFieldShaper.h"
#include "SuiteHistory.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

class BassSpaceProcessor final : public juce::AudioProcessor {
public:
    static constexpr int minEditorWidth = 720;
    static constexpr int minEditorHeight = 520;
    static constexpr int defaultEditorWidth = 840;
    static constexpr int defaultEditorHeight = 620;

    BassSpaceProcessor();
    ~BassSpaceProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destination) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

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

    [[nodiscard]] bool currentBypass() const noexcept {
        return bypassParam_ != nullptr && bypassParam_->load(std::memory_order_relaxed) >= 0.5f;
    }

    [[nodiscard]] bool currentLr8Slope() const noexcept {
        return slopeParam_ != nullptr && slopeParam_->load(std::memory_order_relaxed) >= 0.5f;
    }

    [[nodiscard]] bool currentElliptical() const noexcept {
        return ellipticalParam_ != nullptr && ellipticalParam_->load(std::memory_order_relaxed) >= 0.5f;
    }

    [[nodiscard]] float currentCorrelation() const noexcept {
        return correlation_.load(std::memory_order_relaxed);
    }

    float artworkPhase() const noexcept { return artworkClock_.currentPhase(); }

    [[nodiscard]] int editorWidth() const noexcept { return editorWidth_; }
    [[nodiscard]] int editorHeight() const noexcept { return editorHeight_; }
    void setEditorSize(int width, int height) noexcept;

    void undo();
    void redo();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    juce::UndoManager& undoManager() noexcept { return history_.undo; }

    [[nodiscard]] int activeCompareSlot() const noexcept { return activeSlot_; }
    void selectCompareSlot(int slot);
    void copyCompareSlot(int from, int to);

    alanmarcero::suite::PresetLibrary& presetLibrary() noexcept { return presets_; }
    [[nodiscard]] const juce::String& currentPresetName() const noexcept { return currentPresetName_; }
    void loadPreset(const alanmarcero::suite::PresetEntry& preset);
    [[nodiscard]] alanmarcero::suite::ParameterSnapshot currentParameterValues() const;
    void setUserPresetDirectory(const juce::File& directory);

    juce::AudioProcessorValueTreeState parameters;

private:
    void processAudio(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool forceBypass);
    void restoreParameterTree(juce::ValueTree tree);
    [[nodiscard]] juce::ValueTree captureMemory();
    void restoreMemory(const juce::ValueTree& memory);
    void applyValues(const alanmarcero::suite::ParameterSnapshot& values);

    std::atomic<float>* freqParam_ = nullptr;
    std::atomic<float>* widthParam_ = nullptr;
    std::atomic<float>* upperWidthParam_ = nullptr;
    std::atomic<float>* stereoizeParam_ = nullptr;
    std::atomic<float>* outputDbParam_ = nullptr;
    std::atomic<float>* subDriveParam_ = nullptr;
    std::atomic<float>* upperTiltParam_ = nullptr;
    std::atomic<float>* soloLowParam_ = nullptr;
    std::atomic<float>* bypassParam_ = nullptr;
    std::atomic<float>* slopeParam_ = nullptr;
    std::atomic<float>* ellipticalParam_ = nullptr;
    ArtworkBeatClock artworkClock_;
    double processingRate_ = 48000.0;
    std::atomic<float> correlation_ {0.0f};
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix_;
    juce::AudioBuffer<float> dryBuffer_;
    analog_master::core::StereoFieldShaper shaper_;
    analog_master::core::StereoCeilingGuard outputCeilingGuard_;

    alanmarcero::suite::PresetLibrary presets_;
    std::array<alanmarcero::suite::ParameterSnapshot, 2> slots_;
    int activeSlot_ = 0;
    int currentProgram_ = 0;
    juce::String currentPresetName_;
    int editorWidth_ = defaultEditorWidth;
    int editorHeight_ = defaultEditorHeight;
    alanmarcero::suite::SuiteHistory history_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassSpaceProcessor)
};

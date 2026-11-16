#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class BassSpaceEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit BassSpaceEditor(BassSpaceProcessor&);
    ~BassSpaceEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateValueLabels();

    BassSpaceProcessor& processor_;
    juce::Slider freqSlider_;
    juce::Slider widthSlider_;
    juce::Slider upperWidthSlider_;
    juce::Slider stereoizeSlider_;
    juce::Slider outputDbSlider_;
    juce::Label freqLabel_;
    juce::Label widthLabel_;
    juce::Label upperWidthLabel_;
    juce::Label stereoizeLabel_;
    juce::Label correlationLabel_;
    juce::Label correlationValueLabel_;
    juce::Label freqValueLabel_;
    juce::Label widthValueLabel_;
    juce::Label upperWidthValueLabel_;
    juce::Label stereoizeValueLabel_;
    juce::Label outputDbLabel_;
    juce::Label outputDbValueLabel_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> upperWidthAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stereoizeAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputDbAttachment_;

    juce::Image faceplate_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassSpaceEditor)
};

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

    BassSpaceProcessor& processor_;
    juce::Slider freqSlider_;
    juce::Slider widthSlider_;
    juce::Label freqLabel_;
    juce::Label widthLabel_;
    juce::Label freqValueLabel_;
    juce::Label widthValueLabel_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttachment_;

    juce::Image faceplate_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassSpaceEditor)
};

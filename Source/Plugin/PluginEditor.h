#pragma once

#include "PluginProcessor.h"
#include "SuiteStrip.h"
#include <juce_gui_basics/juce_gui_basics.h>

class BassSpaceTheme final : public juce::LookAndFeel_V4 {
public:
    juce::Label* createSliderTextBox(juce::Slider& slider) override;
};

class BassSpaceEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit BassSpaceEditor(BassSpaceProcessor&);
    ~BassSpaceEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    void updateValueLabels();
    void positionPresetBrowser();

    BassSpaceProcessor& processor_;
    BassSpaceTheme theme_;
    juce::TooltipWindow tooltips_ {this, 600};
    juce::Slider freqSlider_;
    juce::Slider widthSlider_;
    juce::Slider upperWidthSlider_;
    juce::Slider stereoizeSlider_;
    juce::Slider subDriveSlider_;
    juce::Slider outputDbSlider_;
    juce::Slider upperTiltSlider_;
    juce::TextButton soloLowButton_{"SOLO BASS"};
    juce::TextButton slopeButton_{"48 dB LR8"};
    juce::TextButton ellipticalButton_{"ELLIPTICAL"};
    juce::Label freqLabel_;
    juce::Label widthLabel_;
    juce::Label upperWidthLabel_;
    juce::Label stereoizeLabel_;
    juce::Label subDriveLabel_;
    juce::Label upperTiltLabel_;
    juce::Label correlationLabel_;
    juce::Label correlationValueLabel_;
    juce::Label outputDbLabel_;

    alanmarcero::suite::SuiteStrip suiteStrip_;
    alanmarcero::suite::PresetBrowser presetBrowser_;
    juce::Component stereoFieldAnchor_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> upperWidthAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stereoizeAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subDriveAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputDbAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> upperTiltAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloLowAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> slopeAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ellipticalAttachment_;

    std::unique_ptr<juce::Drawable> stereoField_;
    juce::Image faceplate_;

    juce::Rectangle<int> headerArea_;
    juce::Rectangle<int> badgeArea_;
    juce::Rectangle<int> footerArea_;
    juce::Rectangle<int> fieldArea_;
    juce::Rectangle<int> ringArea_;
    juce::Rectangle<int> masterArea_;
    juce::Rectangle<float> correlationTrack_;
    std::array<juce::Line<float>, 2> knobDividers_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassSpaceEditor)
};

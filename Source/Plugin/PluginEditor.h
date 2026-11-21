#pragma once

#include "PluginProcessor.h"
#include "SuiteStrip.h"
#include <juce_gui_basics/juce_gui_basics.h>

class BassSpaceTheme final : public juce::LookAndFeel_V4 {
public:
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    juce::Label* createSliderTextBox(juce::Slider& slider) override;
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                          float startAngle, float endAngle, juce::Slider& slider) override;
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& background,
                              bool highlighted, bool down) override;
};

class FineSlider final : public juce::Slider {
public:
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void setTooltip(const juce::String& newTooltip) override;
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }

private:
    juce::Point<float> lastMouse_;
    juce::Point<float> dragPosition_;
};

class BassSpaceEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit BassSpaceEditor(BassSpaceProcessor&);
    ~BassSpaceEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    // Reads the processor and repaints only the areas whose content changed.
    void refreshDisplay();

private:
    void timerCallback() override { refreshDisplay(); }
    void updateMeter();
    void paintBackground(juce::Graphics& g) const;
    void positionPresetBrowser();

    BassSpaceProcessor& processor_;
    BassSpaceTheme theme_;
    juce::TooltipWindow tooltips_ {this, 600};
    FineSlider freqSlider_;
    FineSlider widthSlider_;
    FineSlider upperWidthSlider_;
    FineSlider stereoizeSlider_;
    FineSlider subDriveSlider_;
    FineSlider outputDbSlider_;
    FineSlider upperTiltSlider_;
    juce::TextButton soloLowButton_{"SOLO BASS"};
    juce::TextButton slopeButton_{"SLOPE 24 dB"};
    juce::TextButton ellipticalButton_{"SIDE-ONLY SPLIT"};
    juce::Label freqLabel_;
    juce::Label widthLabel_;
    juce::Label upperWidthLabel_;
    juce::Label stereoizeLabel_;
    juce::Label subDriveLabel_;
    juce::Label upperTiltLabel_;
    juce::Label correlationLabel_;
    juce::Label correlationValueLabel_;
    juce::Label bassCorrelationLabel_;
    juce::Label bassCorrelationValueLabel_;
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
    juce::Image background_;
    std::array<float, 6> lastField_ {};
    juce::String footerText_;
    bool upperBandDimmed_ = false;

    juce::Rectangle<int> headerArea_;
    juce::Rectangle<int> badgeArea_;
    juce::Rectangle<int> footerArea_;
    juce::Rectangle<int> fieldArea_;
    juce::Rectangle<int> ringArea_;
    juce::Rectangle<int> masterArea_;
    juce::Rectangle<int> meterArea_;
    std::array<juce::Rectangle<float>, 2> correlationTracks_;
    std::array<juce::Line<float>, 2> knobDividers_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassSpaceEditor)
};

#include "PluginEditor.h"
#include "BinaryData.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

const juce::Colour kBackground{ 0xff0d1317 };
const juce::Colour kPanel{ 0xff152026 };
const juce::Colour kPanelEdge{ 0xff283842 };
const juce::Colour kAccentMint{ 0xff48f0c8 };
const juce::Colour kAccentGold{ 0xffffbe3b };
const juce::Colour kTextWhite{ 0xffe6f0f5 };
const juce::Colour kTextDim{ 0xff869ca8 };

void drawGlow(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour) {
    const juce::Graphics::ScopedSaveState saved(g);
    const auto centre = bounds.getCentre();
    const float radius = bounds.getWidth() * 0.5f;
    if (radius <= 1.0f) {
        return;
    }
    g.addTransform(juce::AffineTransform::scale(1.0f, bounds.getHeight() / bounds.getWidth(), centre.x, centre.y));
    g.setGradientFill(juce::ColourGradient(colour, centre, colour.withAlpha(0.0f), centre.translated(radius, 0.0f), true));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
}

} // namespace

BassSpaceEditor::BassSpaceEditor(BassSpaceProcessor& p)
    : AudioProcessorEditor(p), processor_(p) {
    faceplate_ = juce::ImageCache::getFromMemory(
        BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize);

    // Frequency knob
    freqSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    freqSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 22);
    freqSlider_.setColour(juce::Slider::rotarySliderFillColourId, kAccentMint);
    freqSlider_.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    freqSlider_.setColour(juce::Slider::thumbColourId, kTextWhite);
    freqSlider_.setColour(juce::Slider::textBoxTextColourId, kAccentMint);
    freqSlider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    freqSlider_.setDoubleClickReturnValue(true, 120.0);
    freqSlider_.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v)) + " Hz"; };
    addAndMakeVisible(freqSlider_);

    freqLabel_.setText("FREQUENCY", juce::dontSendNotification);
    freqLabel_.setJustificationType(juce::Justification::centred);
    freqLabel_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    freqLabel_.setColour(juce::Label::textColourId, kTextWhite);
    addAndMakeVisible(freqLabel_);

    // Width knob
    widthSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    widthSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 22);
    widthSlider_.setColour(juce::Slider::rotarySliderFillColourId, kAccentGold);
    widthSlider_.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    widthSlider_.setColour(juce::Slider::thumbColourId, kTextWhite);
    widthSlider_.setColour(juce::Slider::textBoxTextColourId, kAccentGold);
    widthSlider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    widthSlider_.setDoubleClickReturnValue(true, 100.0);
    widthSlider_.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v)) + " %"; };
    addAndMakeVisible(widthSlider_);

    widthLabel_.setText("BASS WIDTH", juce::dontSendNotification);
    widthLabel_.setJustificationType(juce::Justification::centred);
    widthLabel_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    widthLabel_.setColour(juce::Label::textColourId, kTextWhite);
    addAndMakeVisible(widthLabel_);

    freqAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kFrequency, freqSlider_);
    widthAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kWidth, widthSlider_);

    setSize(560, 360);
    startTimerHz(30);
}

BassSpaceEditor::~BassSpaceEditor() {
    stopTimer();
}

void BassSpaceEditor::timerCallback() {
    repaint();
}

void BassSpaceEditor::paint(juce::Graphics& g) {
    g.fillAll(kBackground);

    if (!faceplate_.isNull()) {
        g.setOpacity(0.45f);
        g.drawImage(faceplate_, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
    }

    // Outer border
    g.setColour(kPanelEdge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(3.0f), 6.0f, 1.2f);

    // Header
    const auto header = getLocalBounds().reduced(20, 16).removeFromTop(44);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.setColour(kTextWhite);
    g.drawText("open", header, juce::Justification::centredLeft);
    const int openWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "open");
    g.setColour(kAccentMint);
    g.drawText("BassSpace", header.withTrimmedLeft(openWidth + 2), juce::Justification::centredLeft);
    const int totalTitleWidth = openWidth + 2 + juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "BassSpace");

    // Header badge
    const auto badge = header.withTrimmedLeft(totalTitleWidth + 16).withSizeKeepingCentre(190, 20);
    g.setColour(kPanel.brighter(0.08f));
    g.fillRoundedRectangle(badge.toFloat(), 4.0f);
    g.setColour(kAccentMint.withAlpha(0.6f));
    g.drawRoundedRectangle(badge.toFloat(), 4.0f, 1.0f);
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.setColour(kAccentMint);
    g.drawText("SUB STEREO FIELD SHAPER", badge, juce::Justification::centred);

    // Version label right
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(kTextDim);
    g.drawText("v" JucePlugin_VersionString, header, juce::Justification::centredRight);

    // Center Display Monitor
    auto centerArea = juce::Rectangle<float>(200, 95, 160, 175);
    drawGlow(g, centerArea, (processor_.currentWidth() > 105.0f ? kAccentGold : kAccentMint).withAlpha(0.08f));
    g.setColour(kPanel);
    g.fillRoundedRectangle(centerArea, 8.0f);
    g.setColour(kPanelEdge);
    g.drawRoundedRectangle(centerArea, 8.0f, 1.0f);

    // Stereo spread arc visualization
    const auto arcCentre = juce::Point<float>(centerArea.getCentreX(), centerArea.getY() + 72.0f);
    const float currentW = processor_.currentWidth();
    const float spreadAngle = (currentW / 200.0f) * (std::numbers::pi_v<float> * 0.42f);

    // Center mono beam line
    g.setColour(kTextDim.withAlpha(0.35f));
    const float dashes[] = { 4.0f, 4.0f };
    g.drawDashedLine(juce::Line<float>(arcCentre.x, arcCentre.y - 45.0f, arcCentre.x, arcCentre.y + 10.0f),
                     dashes, 2, 1.0f);

    // Stereo vector wings
    juce::Path wings;
    const float wingLength = 48.0f;
    wings.startNewSubPath(arcCentre);
    wings.lineTo(arcCentre.x - std::sin(spreadAngle) * wingLength, arcCentre.y - std::cos(spreadAngle) * wingLength);
    wings.startNewSubPath(arcCentre);
    wings.lineTo(arcCentre.x + std::sin(spreadAngle) * wingLength, arcCentre.y - std::cos(spreadAngle) * wingLength);

    const auto wingCol = currentW < 10.0f ? kAccentMint : (currentW > 105.0f ? kAccentGold : kTextWhite);
    g.setColour(wingCol.withAlpha(0.9f));
    g.strokePath(wings, juce::PathStrokeType(currentW < 10.0f ? 3.5f : 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Center pivot dot
    g.setColour(wingCol);
    g.fillEllipse(arcCentre.x - 3.5f, arcCentre.y - 3.5f, 7.0f, 7.0f);

    // Mode text readout
    juce::String modeText = "100% FLAT";
    juce::Colour modeCol = kTextWhite;
    if (currentW < 5.0f) {
        modeText = "0% PURE MONO";
        modeCol = kAccentMint;
    }
    if (currentW >= 5.0f && currentW < 95.0f) {
        modeText = "MONO FOCUSED";
        modeCol = kAccentMint.withAlpha(0.85f);
    }
    if (currentW > 105.0f) {
        modeText = "STEREO EXPANDED";
        modeCol = kAccentGold;
    }

    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(modeCol);
    g.drawText(modeText, centerArea.removeFromBottom(48).withHeight(18), juce::Justification::centred);

    g.setFont(juce::FontOptions(9.5f));
    g.setColour(kTextDim);
    g.drawText("Sub < " + juce::String(juce::roundToInt(processor_.currentFrequency())) + " Hz",
               centerArea.removeFromBottom(28).withHeight(16), juce::Justification::centred);

    // Footer
    const auto footer = getLocalBounds().removeFromBottom(30);
    g.setFont(juce::FontOptions(10.0f));
    g.setColour(kTextDim.withAlpha(0.7f));
    g.drawText("4th-Order Linkwitz-Riley Crossover  ·  Flat High-End Recombination", footer, juce::Justification::centred);
}

void BassSpaceEditor::resized() {
    const int knobSize = 130;
    const int knobY = 95;

    freqSlider_.setBounds(40, knobY, knobSize, knobSize + 24);
    freqLabel_.setBounds(40, knobY + knobSize + 28, knobSize, 18);

    widthSlider_.setBounds(getWidth() - 40 - knobSize, knobY, knobSize, knobSize + 24);
    widthLabel_.setBounds(getWidth() - 40 - knobSize, knobY + knobSize + 28, knobSize, 18);
}

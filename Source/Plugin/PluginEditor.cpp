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
const juce::Colour kPanelRule{ 0xff31434d };

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
    freqSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    freqSlider_.setColour(juce::Slider::rotarySliderFillColourId, kAccentMint);
    freqSlider_.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    freqSlider_.setColour(juce::Slider::thumbColourId, kTextWhite);
    freqSlider_.setDoubleClickReturnValue(true, 120.0);
    freqSlider_.setTitle("Crossover frequency");
    freqSlider_.setDescription("The low band stays mono below this frequency.");
    freqSlider_.setTooltip("Set the crossover frequency. The low band stays mono below this point.");
    addAndMakeVisible(freqSlider_);

    freqLabel_.setText("Crossover", juce::dontSendNotification);
    freqLabel_.setJustificationType(juce::Justification::centred);
    freqLabel_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    freqLabel_.setColour(juce::Label::textColourId, kTextWhite);
    addAndMakeVisible(freqLabel_);

    freqValueLabel_.setJustificationType(juce::Justification::centred);
    freqValueLabel_.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    freqValueLabel_.setColour(juce::Label::textColourId, kAccentMint);
    addAndMakeVisible(freqValueLabel_);

    // Width knob
    widthSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    widthSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    widthSlider_.setColour(juce::Slider::rotarySliderFillColourId, kAccentGold);
    widthSlider_.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    widthSlider_.setColour(juce::Slider::thumbColourId, kTextWhite);
    widthSlider_.setDoubleClickReturnValue(true, 100.0);
    widthSlider_.setTitle("Stereo width");
    widthSlider_.setDescription("Sets the stereo width above the crossover frequency.");
    widthSlider_.setTooltip("Set the stereo width above the crossover frequency.");
    addAndMakeVisible(widthSlider_);

    widthLabel_.setText("Stereo width", juce::dontSendNotification);
    widthLabel_.setJustificationType(juce::Justification::centred);
    widthLabel_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    widthLabel_.setColour(juce::Label::textColourId, kTextWhite);
    addAndMakeVisible(widthLabel_);

    widthValueLabel_.setJustificationType(juce::Justification::centred);
    widthValueLabel_.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    widthValueLabel_.setColour(juce::Label::textColourId, kAccentGold);
    addAndMakeVisible(widthValueLabel_);

    freqAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kFrequency, freqSlider_);
    widthAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kWidth, widthSlider_);

    outputDbSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputDbSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    outputDbSlider_.setColour(juce::Slider::rotarySliderFillColourId, kAccentMint);
    outputDbSlider_.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    outputDbSlider_.setColour(juce::Slider::thumbColourId, kTextWhite);
    outputDbSlider_.setDoubleClickReturnValue(true, 0.0);
    outputDbSlider_.setTitle("Output level");
    outputDbSlider_.setDescription("Sets the plug-in output level in decibels.");
    outputDbSlider_.setTooltip("Set the plug-in output level in decibels.");
    addAndMakeVisible(outputDbSlider_);

    outputDbAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kOutputDb, outputDbSlider_);

    outputDbLabel_.setText("Output dB", juce::dontSendNotification);
    outputDbLabel_.setJustificationType(juce::Justification::centred);
    outputDbLabel_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    outputDbLabel_.setColour(juce::Label::textColourId, kTextWhite);
    addAndMakeVisible(outputDbLabel_);

    outputDbValueLabel_.setJustificationType(juce::Justification::centred);
    outputDbValueLabel_.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    outputDbValueLabel_.setColour(juce::Label::textColourId, kAccentMint);
    addAndMakeVisible(outputDbValueLabel_);

    setSize(760, 480);
    updateValueLabels();
    startTimerHz(30);
}

BassSpaceEditor::~BassSpaceEditor() {
    stopTimer();
}

void BassSpaceEditor::timerCallback() {
    updateValueLabels();
    repaint();
}

void BassSpaceEditor::updateValueLabels() {
    freqValueLabel_.setText(juce::String(juce::roundToInt(freqSlider_.getValue())) + " Hz", juce::dontSendNotification);
    widthValueLabel_.setText(juce::String(juce::roundToInt(widthSlider_.getValue())) + "%", juce::dontSendNotification);
    outputDbValueLabel_.setText(juce::String(outputDbSlider_.getValue(), 1) + " dB", juce::dontSendNotification);
}

void BassSpaceEditor::paint(juce::Graphics& g) {
    g.fillAll(kBackground);

    if (!faceplate_.isNull()) {
        g.setOpacity(0.24f);
        g.drawImage(faceplate_, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
    }

    g.setColour(kBackground.withAlpha(0.28f));
    g.fillRect(getLocalBounds().reduced(5));

    // Outer border
    g.setColour(kPanelEdge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(3.0f), 5.0f, 1.2f);

    // Header
    const auto header = getLocalBounds().reduced(20, 14).removeFromTop(44);
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

    g.setColour(kPanelRule.withAlpha(0.85f));
    g.drawHorizontalLine(70.0f, 22.0f, static_cast<float>(getWidth() - 22));
    g.setColour(kPanelRule.withAlpha(0.6f));
    g.drawVerticalLine(253.0f, 92.0f, 279.0f);
    g.drawVerticalLine(506.0f, 92.0f, 279.0f);

    // Center Display Monitor
    auto centerArea = juce::Rectangle<float>(270, 88, 220, 192);
    const auto fieldAccent = (processor_.currentWidth() > 105.0f ? kAccentGold : kAccentMint);
    drawGlow(g, centerArea, fieldAccent.withAlpha(0.07f));
    g.setColour(kPanel);
    g.fillRoundedRectangle(centerArea, 5.0f);
    g.setColour(kPanelEdge);
    g.drawRoundedRectangle(centerArea, 5.0f, 1.0f);

    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(kTextDim);
    g.drawText("STEREO FIELD", centerArea.withTrimmedTop(10.0f).withHeight(15.0f), juce::Justification::centred);

    // Stereo spread arc visualization
    const auto arcCentre = juce::Point<float>(centerArea.getCentreX(), centerArea.getY() + 88.0f);
    const float currentW = processor_.currentWidth();
    const float spreadAngle = (currentW / 200.0f) * (std::numbers::pi_v<float> * 0.42f);

    // Center mono beam line
    g.setColour(kTextDim.withAlpha(0.35f));
    const float dashes[] = { 4.0f, 4.0f };
    g.drawDashedLine(juce::Line<float>(arcCentre.x, arcCentre.y - 40.0f, arcCentre.x, arcCentre.y + 11.0f),
                     dashes, 2, 1.0f);

    // Stereo vector wings
    juce::Path wings;
    const float wingLength = 44.0f;
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

    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour(modeCol);
    g.drawText(modeText, centerArea.withY(218.0f).withHeight(17.0f), juce::Justification::centred);

    g.setFont(juce::FontOptions(9.5f));
    g.setColour(kTextDim);
    g.drawText("Low band mono below " + juce::String(juce::roundToInt(processor_.currentFrequency())) + " Hz",
               centerArea.withY(241.0f).withHeight(16.0f), juce::Justification::centred);

    const auto masterStrip = juce::Rectangle<float>(220.0f, 296.0f, 320.0f, 144.0f);
    g.setColour(kPanel.withAlpha(0.78f));
    g.fillRoundedRectangle(masterStrip, 5.0f);
    g.setColour(kPanelEdge.withAlpha(0.9f));
    g.drawRoundedRectangle(masterStrip, 5.0f, 1.0f);
    g.setColour(kAccentMint.withAlpha(0.58f));
    g.fillRoundedRectangle(masterStrip.withTrimmedTop(0.0f).withHeight(2.0f).reduced(1.0f, 0.0f), 1.0f);

    // Footer
    const auto footer = getLocalBounds().removeFromBottom(30);
    g.setFont(juce::FontOptions(9.5f));
    g.setColour(kTextDim);
    g.drawText("4th-order Linkwitz-Riley  |  low band mono  |  high band width", footer, juce::Justification::centred);
}

void BassSpaceEditor::resized() {
    updateValueLabels();
    const int knobSize = 132;
    const int knobY = 110;
    const int sideColumnWidth = 188;

    freqLabel_.setBounds(36, 91, sideColumnWidth, 18);
    freqSlider_.setBounds(64, knobY, knobSize, knobSize);
    freqValueLabel_.setBounds(36, knobY + knobSize + 2, sideColumnWidth, 22);

    const int widthX = getWidth() - 36 - sideColumnWidth;
    widthLabel_.setBounds(widthX, 91, sideColumnWidth, 18);
    widthSlider_.setBounds(widthX + (sideColumnWidth - knobSize) / 2, knobY, knobSize, knobSize);
    widthValueLabel_.setBounds(widthX, knobY + knobSize + 2, sideColumnWidth, 22);

    outputDbLabel_.setBounds((getWidth() - 180) / 2, 302, 180, 18);
    outputDbSlider_.setBounds((getWidth() - 112) / 2, 326, 112, 78);
    outputDbValueLabel_.setBounds((getWidth() - 180) / 2, 406, 180, 22);
}

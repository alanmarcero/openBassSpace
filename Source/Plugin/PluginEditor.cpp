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
const juce::Colour kTextDim{ 0xffa3b6bf };
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
    theme_.setColour(juce::TooltipWindow::backgroundColourId, kPanel);
    theme_.setColour(juce::TooltipWindow::textColourId, kTextWhite);
    theme_.setColour(juce::TooltipWindow::outlineColourId, kPanelRule);
    setLookAndFeel(&theme_);
    faceplate_ = juce::ImageCache::getFromMemory(
        BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize);

    stereoField_ = juce::Drawable::createFromImageData(BinaryData::stereofield_svg, BinaryData::stereofield_svgSize);

    // Frequency knob
    freqSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    freqSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    freqSlider_.setColour(juce::Slider::rotarySliderFillColourId, kAccentMint);
    freqSlider_.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    freqSlider_.setColour(juce::Slider::thumbColourId, kTextWhite);
    freqSlider_.setDoubleClickReturnValue(true, 120.0);
    freqSlider_.setTitle("Crossover frequency");
    freqSlider_.setDescription("Sets the boundary between the bass and upper stereo width controls.");
    freqSlider_.setTooltip("Set the boundary between Bass width and Upper width.");
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

    // Bass width knob
    widthSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    widthSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    widthSlider_.setColour(juce::Slider::rotarySliderFillColourId, kAccentGold);
    widthSlider_.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    widthSlider_.setColour(juce::Slider::thumbColourId, kTextWhite);
    widthSlider_.setDoubleClickReturnValue(true, 0.0);
    widthSlider_.setTitle("Bass stereo width");
    widthSlider_.setDescription("Sets the stereo width below the crossover frequency.");
    widthSlider_.setTooltip("Set the bass stereo width below the crossover frequency.");
    addAndMakeVisible(widthSlider_);

    widthLabel_.setText("Bass width", juce::dontSendNotification);
    widthLabel_.setJustificationType(juce::Justification::centred);
    widthLabel_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    widthLabel_.setColour(juce::Label::textColourId, kTextWhite);
    addAndMakeVisible(widthLabel_);

    widthValueLabel_.setJustificationType(juce::Justification::centred);
    widthValueLabel_.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    widthValueLabel_.setColour(juce::Label::textColourId, kAccentGold);
    addAndMakeVisible(widthValueLabel_);

    upperWidthSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    upperWidthSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    upperWidthSlider_.setColour(juce::Slider::rotarySliderFillColourId, kAccentGold);
    upperWidthSlider_.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    upperWidthSlider_.setColour(juce::Slider::thumbColourId, kTextWhite);
    upperWidthSlider_.setDoubleClickReturnValue(true, 100.0);
    upperWidthSlider_.setTitle("Upper-band width");
    upperWidthSlider_.setDescription("Sets stereo width above the crossover frequency.");
    upperWidthSlider_.setTooltip("Set stereo width above the crossover frequency.");
    addAndMakeVisible(upperWidthSlider_);

    upperWidthLabel_.setText("Upper width", juce::dontSendNotification);
    upperWidthLabel_.setJustificationType(juce::Justification::centred);
    upperWidthLabel_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    upperWidthLabel_.setColour(juce::Label::textColourId, kTextWhite);
    addAndMakeVisible(upperWidthLabel_);

    upperWidthValueLabel_.setJustificationType(juce::Justification::centred);
    upperWidthValueLabel_.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    upperWidthValueLabel_.setColour(juce::Label::textColourId, kAccentGold);
    addAndMakeVisible(upperWidthValueLabel_);

    stereoizeSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    stereoizeSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    stereoizeSlider_.setColour(juce::Slider::rotarySliderFillColourId, kAccentMint);
    stereoizeSlider_.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    stereoizeSlider_.setColour(juce::Slider::thumbColourId, kTextWhite);
    stereoizeSlider_.setDoubleClickReturnValue(true, 0.0);
    stereoizeSlider_.setTitle("Stereoize");
    stereoizeSlider_.setDescription("Adds a mono-compatible decorrelated side signal above the crossover.");
    stereoizeSlider_.setTooltip("Add a decorrelated side signal above the crossover. The bass remains centered.");
    addAndMakeVisible(stereoizeSlider_);

    stereoizeLabel_.setText("Stereoize", juce::dontSendNotification);
    stereoizeLabel_.setJustificationType(juce::Justification::centred);
    stereoizeLabel_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    stereoizeLabel_.setColour(juce::Label::textColourId, kTextWhite);
    addAndMakeVisible(stereoizeLabel_);

    stereoizeValueLabel_.setJustificationType(juce::Justification::centred);
    stereoizeValueLabel_.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    stereoizeValueLabel_.setColour(juce::Label::textColourId, kAccentMint);
    addAndMakeVisible(stereoizeValueLabel_);

    correlationLabel_.setText("Correlation", juce::dontSendNotification);
    correlationLabel_.setJustificationType(juce::Justification::centredLeft);
    correlationLabel_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    correlationLabel_.setColour(juce::Label::textColourId, kTextDim);
    addAndMakeVisible(correlationLabel_);

    correlationValueLabel_.setComponentID("correlationValue");
    correlationValueLabel_.setJustificationType(juce::Justification::centredRight);
    correlationValueLabel_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    correlationValueLabel_.setColour(juce::Label::textColourId, kAccentMint);
    addAndMakeVisible(correlationValueLabel_);

    freqAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kFrequency, freqSlider_);
    widthAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kWidth, widthSlider_);
    upperWidthAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kUpperWidth, upperWidthSlider_);
    stereoizeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kStereoize, stereoizeSlider_);

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
    outputDbLabel_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    outputDbLabel_.setColour(juce::Label::textColourId, kTextWhite);
    addAndMakeVisible(outputDbLabel_);

    outputDbValueLabel_.setJustificationType(juce::Justification::centred);
    outputDbValueLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    outputDbValueLabel_.setColour(juce::Label::textColourId, kAccentMint);
    addAndMakeVisible(outputDbValueLabel_);

    subDriveSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    subDriveSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    subDriveSlider_.setColour(juce::Slider::rotarySliderFillColourId, kAccentMint);
    subDriveSlider_.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    subDriveSlider_.setColour(juce::Slider::thumbColourId, kTextWhite);
    subDriveSlider_.setDoubleClickReturnValue(true, 0.0);
    subDriveSlider_.setTitle("Sub harmonic drive");
    subDriveSlider_.setDescription("Adds mono-locked harmonic warmth to the bass band below the crossover.");
    subDriveSlider_.setTooltip("Add mono-locked harmonic saturation to the low band below the crossover.");
    addAndMakeVisible(subDriveSlider_);

    subDriveAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kSubDrive, subDriveSlider_);

    subDriveLabel_.setText("Sub drive", juce::dontSendNotification);
    subDriveLabel_.setJustificationType(juce::Justification::centred);
    subDriveLabel_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    subDriveLabel_.setColour(juce::Label::textColourId, kTextWhite);
    addAndMakeVisible(subDriveLabel_);

    subDriveValueLabel_.setJustificationType(juce::Justification::centred);
    subDriveValueLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    subDriveValueLabel_.setColour(juce::Label::textColourId, kAccentMint);
    addAndMakeVisible(subDriveValueLabel_);

    upperTiltSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    upperTiltSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    upperTiltSlider_.setColour(juce::Slider::rotarySliderFillColourId, kAccentGold);
    upperTiltSlider_.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    upperTiltSlider_.setColour(juce::Slider::thumbColourId, kTextWhite);
    upperTiltSlider_.setDoubleClickReturnValue(true, 0.0);
    upperTiltSlider_.setTitle("Upper air tilt");
    upperTiltSlider_.setDescription("Tilts the upper stereo band brightness from -6 dB to +6 dB.");
    upperTiltSlider_.setTooltip("Tilt upper-band high frequencies (-6 dB dark to +6 dB air).");
    addAndMakeVisible(upperTiltSlider_);

    upperTiltAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kUpperTilt, upperTiltSlider_);

    upperTiltLabel_.setText("Upper tilt", juce::dontSendNotification);
    upperTiltLabel_.setJustificationType(juce::Justification::centred);
    upperTiltLabel_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    upperTiltLabel_.setColour(juce::Label::textColourId, kTextWhite);
    addAndMakeVisible(upperTiltLabel_);

    upperTiltValueLabel_.setJustificationType(juce::Justification::centred);
    upperTiltValueLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    upperTiltValueLabel_.setColour(juce::Label::textColourId, kAccentGold);
    addAndMakeVisible(upperTiltValueLabel_);

    soloLowButton_.setClickingTogglesState(true);
    soloLowButton_.setColour(juce::TextButton::buttonColourId, kPanel);
    soloLowButton_.setColour(juce::TextButton::buttonOnColourId, kAccentMint);
    soloLowButton_.setColour(juce::TextButton::textColourOffId, kTextWhite);
    soloLowButton_.setColour(juce::TextButton::textColourOnId, kBackground);
    soloLowButton_.setTitle("Solo bass band");
    soloLowButton_.setDescription("Auditions only the low-frequency band below the crossover.");
    soloLowButton_.setTooltip("Audition the isolated bass band below the crossover frequency.");
    addAndMakeVisible(soloLowButton_);

    soloLowAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.parameters, bass_space::params::kSoloLow, soloLowButton_);

    setSize(760, 480);
    updateValueLabels();
    startTimerHz(30);
}

BassSpaceEditor::~BassSpaceEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void BassSpaceEditor::timerCallback() {
    updateValueLabels();
    repaint();
}

void BassSpaceEditor::updateValueLabels() {
    freqValueLabel_.setText(juce::String(juce::roundToInt(freqSlider_.getValue())) + " Hz", juce::dontSendNotification);
    widthValueLabel_.setText(juce::String(juce::roundToInt(widthSlider_.getValue())) + "%", juce::dontSendNotification);
    upperWidthValueLabel_.setText(juce::String(juce::roundToInt(upperWidthSlider_.getValue())) + "%", juce::dontSendNotification);
    stereoizeValueLabel_.setText(juce::String(juce::roundToInt(stereoizeSlider_.getValue())) + "%", juce::dontSendNotification);
    correlationValueLabel_.setText(juce::String(processor_.currentCorrelation(), 2), juce::dontSendNotification);
    subDriveValueLabel_.setText(juce::String(juce::roundToInt(subDriveSlider_.getValue())) + "%", juce::dontSendNotification);
    outputDbValueLabel_.setText(juce::String(outputDbSlider_.getValue(), 1) + " dB", juce::dontSendNotification);
    const double tiltVal = upperTiltSlider_.getValue();
    upperTiltValueLabel_.setText((tiltVal > 0.05 ? "+" : "") + juce::String(std::abs(tiltVal) < 0.05 ? 0.0 : tiltVal, 1) + " dB", juce::dontSendNotification);
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

    // Header badge
    const auto badge = juce::Rectangle<int>(214, 26, 190, 20);
    g.setColour(kPanel.brighter(0.08f));
    g.fillRoundedRectangle(badge.toFloat(), 4.0f);
    g.setColour(kAccentMint.withAlpha(0.6f));
    g.drawRoundedRectangle(badge.toFloat(), 4.0f, 1.0f);
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.setColour(kAccentMint);
    g.drawText("SUB STEREO FIELD SHAPER", badge, juce::Justification::centred);

    // Version label right
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(kTextDim);
    g.drawText("v" JucePlugin_VersionString, header, juce::Justification::centredRight);

    g.setColour(kPanelRule.withAlpha(0.85f));
    g.drawHorizontalLine(70.0f, 22.0f, static_cast<float>(getWidth() - 22));
    g.setColour(kPanelRule.withAlpha(0.6f));
    g.drawVerticalLine(253.0f, 92.0f, 279.0f);
    g.drawVerticalLine(506.0f, 92.0f, 279.0f);

    // Center Display Monitor
    auto centerArea = juce::Rectangle<float>(270, 88, 220, 192);
    const auto fieldAccent = (processor_.currentUpperWidth() > 105.0f ? kAccentGold : kAccentMint);
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
    const float currentW = processor_.currentUpperWidth();
    const float spreadAngle = (currentW / 200.0f) * (std::numbers::pi_v<float> * 0.42f);

    // The scope rings breathe on quarter notes behind the width indicator.
    if (stereoField_) {
        const float phase = processor_.artworkPhase();
        const float pulse = (1.0f - phase) * (1.0f - phase);
        auto rings = juce::Rectangle<float>(134.0f + 20.0f * pulse, 77.0f + 9.0f * pulse)
                         .withCentre(arcCentre.translated(0.0f, -3.0f));
        stereoField_->drawWithin(g, rings, juce::RectanglePlacement::centred, 0.46f + 0.40f * pulse);
    }

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
        modeText = "UPPER BAND MONO";
        modeCol = kAccentMint;
    }
    if (currentW >= 5.0f && currentW < 95.0f) {
        modeText = "UPPER BAND NARROW";
        modeCol = kAccentMint.withAlpha(0.85f);
    }
    if (currentW > 105.0f) {
        modeText = "UPPER BAND WIDE";
        modeCol = kAccentGold;
    }

    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour(modeCol);
    g.drawText(modeText, centerArea.withY(206.0f).withHeight(16.0f), juce::Justification::centred);

    g.setFont(juce::FontOptions(11.0f));
    g.setColour(kTextDim);
    g.drawText("Bass " + juce::String(juce::roundToInt(processor_.currentWidth())) + "%  |  upper band above "
                   + juce::String(juce::roundToInt(processor_.currentFrequency())) + " Hz",
               centerArea.withY(224.0f).withHeight(16.0f), juce::Justification::centred);

    const float correlation = juce::jlimit(-1.0f, 1.0f, processor_.currentCorrelation());
    const juce::Rectangle<float> correlationTrack {292.0f, 265.0f, 176.0f, 5.0f};
    g.setColour(kPanelEdge);
    g.fillRoundedRectangle(correlationTrack, 2.5f);
    g.setColour(correlation < 0.0f ? kAccentGold : kAccentMint);
    const float correlationEnd = correlationTrack.getCentreX() + correlation * 88.0f;
    g.fillRoundedRectangle(juce::Rectangle<float>(std::min(correlationTrack.getCentreX(), correlationEnd),
                                                  correlationTrack.getY(),
                                                  std::abs(correlationEnd - correlationTrack.getCentreX()),
                                                  correlationTrack.getHeight()), 2.5f);

    const auto masterStrip = juce::Rectangle<float>(220.0f, 296.0f, 320.0f, 144.0f);
    g.setColour(kPanel.withAlpha(0.78f));
    g.fillRoundedRectangle(masterStrip, 5.0f);
    g.setColour(kPanelEdge.withAlpha(0.9f));
    g.drawRoundedRectangle(masterStrip, 5.0f, 1.0f);
    g.setColour(kAccentMint.withAlpha(0.58f));
    g.fillRoundedRectangle(masterStrip.withTrimmedTop(0.0f).withHeight(2.0f).reduced(1.0f, 0.0f), 1.0f);

    // Footer
    const auto footer = getLocalBounds().removeFromBottom(30);
    g.setFont(juce::FontOptions(11.0f));
    g.setColour(kTextDim);
    g.drawText("4th-order Linkwitz-Riley  |  independent bass and upper width  |  mono-compatible stereoize",
               footer, juce::Justification::centred);
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

    upperWidthLabel_.setBounds(widthX, 272, sideColumnWidth, 18);
    upperWidthSlider_.setBounds(widthX + (sideColumnWidth - 116) / 2, 290, 116, 116);
    upperWidthValueLabel_.setBounds(widthX, 406, sideColumnWidth, 22);

    stereoizeLabel_.setBounds(36, 272, sideColumnWidth, 18);
    stereoizeSlider_.setBounds(36 + (sideColumnWidth - 116) / 2, 290, 116, 116);
    stereoizeValueLabel_.setBounds(36, 406, sideColumnWidth, 22);

    correlationLabel_.setBounds(292, 245, 96, 16);
    correlationValueLabel_.setBounds(396, 245, 72, 16);

    soloLowButton_.setBounds(564, 18, 108, 36);

    subDriveLabel_.setBounds(226, 304, 96, 18);
    subDriveSlider_.setBounds(236, 326, 76, 76);
    subDriveValueLabel_.setBounds(226, 406, 96, 22);

    outputDbLabel_.setBounds(332, 304, 96, 18);
    outputDbSlider_.setBounds(342, 326, 76, 76);
    outputDbValueLabel_.setBounds(332, 406, 96, 22);

    upperTiltLabel_.setBounds(438, 304, 96, 18);
    upperTiltSlider_.setBounds(448, 326, 76, 76);
    upperTiltValueLabel_.setBounds(438, 406, 96, 22);
}

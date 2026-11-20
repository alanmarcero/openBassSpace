#include "PluginEditor.h"
#include "BinaryData.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
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

void configureKnob(juce::Slider& slider, juce::Colour fill, double doubleClickValue,
                   const juce::String& title, const juce::String& description, const juce::String& tooltip) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour(juce::Slider::rotarySliderFillColourId, fill);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    slider.setColour(juce::Slider::thumbColourId, kTextWhite);
    slider.setDoubleClickReturnValue(true, doubleClickValue);
    slider.setTitle(title);
    slider.setDescription(description);
    slider.setTooltip(tooltip);
    slider.setWantsKeyboardFocus(false);
}

void configureName(juce::Label& label, const juce::String& text) {
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, kTextWhite);
}

void configureValue(juce::Label& label, juce::Colour colour) {
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, colour);
}

void configureModeButton(juce::TextButton& button, juce::Colour activeColour,
                         const juce::String& title, const juce::String& description, const juce::String& tooltip) {
    button.setClickingTogglesState(true);
    button.setColour(juce::TextButton::buttonColourId, kPanel);
    button.setColour(juce::TextButton::buttonOnColourId, activeColour);
    button.setColour(juce::TextButton::textColourOffId, kTextWhite);
    button.setColour(juce::TextButton::textColourOnId, kBackground);
    button.setTitle(title);
    button.setDescription(description);
    button.setTooltip(tooltip);
    button.setWantsKeyboardFocus(false);
}

void placeKnob(juce::Rectangle<int> cell, juce::Label& name, juce::Slider& slider, juce::Label& value) {
    name.setBounds(cell.removeFromTop(16));
    value.setBounds(cell.removeFromBottom(18));
    const int available = juce::jmin(cell.getWidth(), cell.getHeight());
    const int knob = juce::jlimit(48, available, available - 2);
    slider.setBounds(cell.withSizeKeepingCentre(knob, knob));
}

} // namespace

BassSpaceEditor::BassSpaceEditor(BassSpaceProcessor& p)
    : AudioProcessorEditor(p),
      processor_(p),
      suiteStrip_(p.parameters, bass_space::params::kBypass),
      presetBrowser_(p.presetLibrary(),
                     [this] { return processor_.currentParameterValues(); },
                     [this](const alanmarcero::suite::PresetEntry& entry) {
                         processor_.loadPreset(entry);
                         presetBrowser_.setVisible(false);
                         suiteStrip_.refresh();
                     }) {
    theme_.setColour(juce::TooltipWindow::backgroundColourId, kPanel);
    theme_.setColour(juce::TooltipWindow::textColourId, kTextWhite);
    theme_.setColour(juce::TooltipWindow::outlineColourId, kPanelRule);
    setLookAndFeel(&theme_);
    faceplate_ = juce::ImageCache::getFromMemory(
        BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize);
    stereoField_ = juce::Drawable::createFromImageData(BinaryData::stereofield_svg, BinaryData::stereofield_svgSize);

    configureKnob(freqSlider_, kAccentMint, 120.0, "Crossover frequency",
                  "Sets the boundary between the bass and upper stereo width controls.",
                  "Set the boundary between Bass width and Upper width.");
    configureName(freqLabel_, "Crossover");
    configureValue(freqValueLabel_, kAccentMint);

    configureKnob(widthSlider_, kAccentGold, 0.0, "Bass stereo width",
                  "Sets the stereo width below the crossover frequency.",
                  "Set the bass stereo width below the crossover frequency.");
    configureName(widthLabel_, "Bass width");
    configureValue(widthValueLabel_, kAccentGold);

    configureKnob(upperWidthSlider_, kAccentGold, 100.0, "Upper-band width",
                  "Sets stereo width above the crossover frequency.",
                  "Set stereo width above the crossover frequency.");
    configureName(upperWidthLabel_, "Upper width");
    configureValue(upperWidthValueLabel_, kAccentGold);

    configureKnob(stereoizeSlider_, kAccentMint, 0.0, "Stereoize",
                  "Adds a mono-compatible decorrelated side signal above the crossover.",
                  "Add a decorrelated side signal above the crossover. The bass remains centered.");
    configureName(stereoizeLabel_, "Stereoize");
    configureValue(stereoizeValueLabel_, kAccentMint);

    correlationLabel_.setText("Correlation", juce::dontSendNotification);
    correlationLabel_.setJustificationType(juce::Justification::centredLeft);
    correlationLabel_.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    correlationLabel_.setColour(juce::Label::textColourId, kTextDim);
    correlationValueLabel_.setComponentID("correlationValue");
    correlationValueLabel_.setJustificationType(juce::Justification::centredRight);
    correlationValueLabel_.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    correlationValueLabel_.setColour(juce::Label::textColourId, kAccentMint);

    configureKnob(outputDbSlider_, kAccentMint, 0.0, "Output level",
                  "Sets the plug-in output level in decibels.",
                  "Set the plug-in output level in decibels.");
    configureName(outputDbLabel_, "Output dB");
    configureValue(outputDbValueLabel_, kAccentMint);

    configureKnob(subDriveSlider_, kAccentMint, 0.0, "Sub harmonic drive",
                  "Adds mono-locked sub-octave and harmonic warmth to the bass band.",
                  "Blend mono-locked sub-octave synthesis and ADAA saturation into the bass band.");
    configureName(subDriveLabel_, "Sub drive");
    configureValue(subDriveValueLabel_, kAccentMint);

    configureKnob(upperTiltSlider_, kAccentGold, 0.0, "Upper air tilt",
                  "Tilts the upper stereo band brightness from -6 dB to +6 dB.",
                  "Tilt upper-band high frequencies (-6 dB dark to +6 dB air).");
    configureName(upperTiltLabel_, "Upper tilt");
    configureValue(upperTiltValueLabel_, kAccentGold);

    configureModeButton(soloLowButton_, kAccentMint, "Solo bass band",
                        "Auditions only the low-frequency band below the crossover.",
                        "Audition the isolated bass band below the crossover frequency.");
    configureModeButton(slopeButton_, kAccentGold, "48 dB/oct LR8 crossover",
                        "Switches crossover slope between 24 dB/oct LR4 and 48 dB/oct LR8.",
                        "Enable 8th-order 48 dB/oct Linkwitz-Riley sub/mid separation.");
    configureModeButton(ellipticalButton_, kAccentMint, "Elliptical zero-phase Mid",
                        "High-passes only the Side channel while keeping Mid 100% phase-linear.",
                        "Enable mastering Elliptical EQ topology for zero-phase-shift Mid transients.");

    for (auto* component : std::initializer_list<juce::Component*>{
             &freqSlider_, &freqLabel_, &freqValueLabel_,
             &widthSlider_, &widthLabel_, &widthValueLabel_,
             &upperWidthSlider_, &upperWidthLabel_, &upperWidthValueLabel_,
             &stereoizeSlider_, &stereoizeLabel_, &stereoizeValueLabel_,
             &correlationLabel_, &correlationValueLabel_,
             &outputDbSlider_, &outputDbLabel_, &outputDbValueLabel_,
             &subDriveSlider_, &subDriveLabel_, &subDriveValueLabel_,
             &upperTiltSlider_, &upperTiltLabel_, &upperTiltValueLabel_,
             &soloLowButton_, &slopeButton_, &ellipticalButton_, &suiteStrip_ }) {
        addAndMakeVisible(component);
    }

    stereoFieldAnchor_.setComponentID("stereoField");
    stereoFieldAnchor_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(stereoFieldAnchor_);
    addChildComponent(presetBrowser_);

    freqAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kFrequency, freqSlider_);
    widthAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kWidth, widthSlider_);
    upperWidthAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kUpperWidth, upperWidthSlider_);
    stereoizeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kStereoize, stereoizeSlider_);
    outputDbAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kOutputDb, outputDbSlider_);
    subDriveAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kSubDrive, subDriveSlider_);
    upperTiltAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.parameters, bass_space::params::kUpperTilt, upperTiltSlider_);
    soloLowAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.parameters, bass_space::params::kSoloLow, soloLowButton_);
    slopeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.parameters, bass_space::params::kSlope, slopeButton_);
    ellipticalAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.parameters, bass_space::params::kElliptical, ellipticalButton_);

    presetBrowser_.onChanged = [this] { suiteStrip_.refresh(); };
    alanmarcero::suite::SuiteActions actions;
    actions.undo = [this] { processor_.undo(); suiteStrip_.refresh(); };
    actions.redo = [this] { processor_.redo(); suiteStrip_.refresh(); };
    actions.canUndo = [this] { return processor_.canUndo(); };
    actions.canRedo = [this] { return processor_.canRedo(); };
    actions.selectA = [this] { processor_.selectCompareSlot(0); };
    actions.selectB = [this] { processor_.selectCompareSlot(1); };
    actions.copyAtoB = [this] { processor_.copyCompareSlot(0, 1); };
    actions.copyBtoA = [this] { processor_.copyCompareSlot(1, 0); };
    actions.activeSlot = [this] { return processor_.activeCompareSlot(); };
    actions.presetName = [this] { return processor_.currentPresetName(); };
    actions.togglePresetBrowser = [this] {
        presetBrowser_.setVisible(!presetBrowser_.isVisible());
        if (presetBrowser_.isVisible()) {
            presetBrowser_.refresh();
            presetBrowser_.toFront(true);
            positionPresetBrowser();
        }
    };
    suiteStrip_.setActions(std::move(actions));

    setWantsKeyboardFocus(true);
    const int savedWidth = processor_.editorWidth();
    const int savedHeight = processor_.editorHeight();
    setResizable(true, true);
    setResizeLimits(BassSpaceProcessor::minEditorWidth, BassSpaceProcessor::minEditorHeight, 1800, 1400);
    setSize(savedWidth, savedHeight);
    updateValueLabels();
    startTimerHz(30);
}

BassSpaceEditor::~BassSpaceEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

bool BassSpaceEditor::keyPressed(const juce::KeyPress& key) {
    const auto code = key.getKeyCode();
    const bool undoKey = code == 'z' || code == 'Z';
    const bool redoKey = code == 'y' || code == 'Y';
    if (!key.getModifiers().isCommandDown() || key.getModifiers().isAltDown()) {
        return false;
    }
    if (undoKey && key.getModifiers().isShiftDown()) {
        processor_.redo();
        suiteStrip_.refresh();
        return true;
    }
    if (undoKey) {
        processor_.undo();
        suiteStrip_.refresh();
        return true;
    }
    if (redoKey) {
        processor_.redo();
        suiteStrip_.refresh();
        return true;
    }
    return false;
}

void BassSpaceEditor::timerCallback() {
    updateValueLabels();
    suiteStrip_.refresh();
    if (!ringArea_.isEmpty()) {
        repaint(ringArea_.getUnion(correlationTrack_.getSmallestIntegerContainer()).getUnion(footerArea_));
    }
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
    upperTiltValueLabel_.setText((tiltVal > 0.05 ? "+" : "") + juce::String(std::abs(tiltVal) < 0.05 ? 0.0 : tiltVal, 1) + " dB",
                                 juce::dontSendNotification);
}

void BassSpaceEditor::positionPresetBrowser() {
    auto anchor = getLocalArea(&suiteStrip_, suiteStrip_.presetButtonBounds());
    const int browserWidth = juce::jmin(340, getWidth() - 24);
    const int x = juce::jlimit(8, juce::jmax(8, getWidth() - browserWidth - 8), anchor.getRight() - browserWidth);
    const int height = juce::jmin(260, juce::jmax(160, getHeight() - anchor.getBottom() - 16));
    presetBrowser_.setBounds(x, anchor.getBottom() + 4, browserWidth, height);
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
    g.setColour(kPanelEdge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(3.0f), 5.0f, 1.2f);

    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.setColour(kTextWhite);
    g.drawText("open", headerArea_, juce::Justification::centredLeft);
    const int openWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "open");
    g.setColour(kAccentMint);
    g.drawText("BassSpace", headerArea_.withTrimmedLeft(openWidth + 2), juce::Justification::centredLeft);

    g.setColour(kPanel.brighter(0.08f));
    g.fillRoundedRectangle(badgeArea_.toFloat(), 4.0f);
    g.setColour(kAccentMint.withAlpha(0.6f));
    g.drawRoundedRectangle(badgeArea_.toFloat(), 4.0f, 1.0f);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(kAccentMint);
    g.drawText("SUB STEREO FIELD SHAPER", badgeArea_, juce::Justification::centred);

    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(kTextDim);
    g.drawText("v" JucePlugin_VersionString, headerArea_, juce::Justification::centredRight);

    if (fieldArea_.isEmpty()) {
        return;
    }

    const auto centerArea = fieldArea_.toFloat();
    const auto fieldAccent = (processor_.currentUpperWidth() > 105.0f ? kAccentGold : kAccentMint);
    drawGlow(g, centerArea, fieldAccent.withAlpha(0.07f));
    g.setColour(kPanel);
    g.fillRoundedRectangle(centerArea, 5.0f);
    g.setColour(kPanelEdge);
    g.drawRoundedRectangle(centerArea, 5.0f, 1.0f);

    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(kTextDim);
    g.drawText("STEREO FIELD", ringArea_.withHeight(16), juce::Justification::centred);

    const auto arcCentre = ringArea_.toFloat().getCentre().translated(
        0.0f, static_cast<float>(ringArea_.getHeight()) * 0.08f);
    const float currentW = processor_.currentUpperWidth();
    const float spreadAngle = (currentW / 200.0f) * (std::numbers::pi_v<float> * 0.42f);

    if (stereoField_) {
        const float phase = processor_.artworkPhase();
        const float pulse = (1.0f - phase) * (1.0f - phase);
        const float ringSize = juce::jmin(static_cast<float>(ringArea_.getWidth()) * 0.72f,
                                          static_cast<float>(ringArea_.getHeight()) * 0.78f);
        auto rings = juce::Rectangle<float>(ringSize * (0.72f + 0.12f * pulse), ringSize * (0.42f + 0.08f * pulse))
                         .withCentre(arcCentre);
        stereoField_->drawWithin(g, rings, juce::RectanglePlacement::centred, 0.46f + 0.40f * pulse);
    }

    g.setColour(kTextDim.withAlpha(0.35f));
    const float dashes[] = { 4.0f, 4.0f };
    const float beam = juce::jmin(40.0f, static_cast<float>(ringArea_.getHeight()) * 0.22f);
    g.drawDashedLine(juce::Line<float>(arcCentre.x, arcCentre.y - beam, arcCentre.x, arcCentre.y + beam * 0.28f),
                     dashes, 2, 1.0f);

    juce::Path wings;
    const float wingLength = juce::jlimit(28.0f, 64.0f, static_cast<float>(ringArea_.getHeight()) * 0.28f);
    wings.startNewSubPath(arcCentre);
    wings.lineTo(arcCentre.x - std::sin(spreadAngle) * wingLength, arcCentre.y - std::cos(spreadAngle) * wingLength);
    wings.startNewSubPath(arcCentre);
    wings.lineTo(arcCentre.x + std::sin(spreadAngle) * wingLength, arcCentre.y - std::cos(spreadAngle) * wingLength);

    const auto wingCol = currentW < 10.0f ? kAccentMint : (currentW > 105.0f ? kAccentGold : kTextWhite);
    g.setColour(wingCol.withAlpha(0.9f));
    g.strokePath(wings, juce::PathStrokeType(currentW < 10.0f ? 3.5f : 2.5f,
                                             juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(wingCol);
    g.fillEllipse(arcCentre.x - 3.5f, arcCentre.y - 3.5f, 7.0f, 7.0f);

    juce::String modeText = "100% FLAT";
    juce::Colour modeCol = kTextWhite;
    if (currentW < 5.0f) {
        modeText = "UPPER BAND MONO";
        modeCol = kAccentMint;
    } else if (currentW < 95.0f) {
        modeText = "UPPER BAND NARROW";
        modeCol = kAccentMint.withAlpha(0.85f);
    } else if (currentW > 105.0f) {
        modeText = "UPPER BAND WIDE";
        modeCol = kAccentGold;
    }

    auto modeArea = ringArea_.toFloat();
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour(modeCol);
    g.drawText(modeText, modeArea.removeFromBottom(16.0f), juce::Justification::centred);
    g.setFont(juce::FontOptions(11.0f));
    g.setColour(kTextDim);
    g.drawText("Bass " + juce::String(juce::roundToInt(processor_.currentWidth())) + "%  |  upper band above "
                   + juce::String(juce::roundToInt(processor_.currentFrequency())) + " Hz",
               modeArea.removeFromBottom(16.0f), juce::Justification::centred);

    const float correlation = juce::jlimit(-1.0f, 1.0f, processor_.currentCorrelation());
    g.setColour(kPanelEdge);
    g.fillRoundedRectangle(correlationTrack_, 3.0f);
    g.setColour(correlation < 0.0f ? kAccentGold : kAccentMint);
    const float correlationEnd = correlationTrack_.getCentreX() + correlation * correlationTrack_.getWidth() * 0.5f;
    g.fillRoundedRectangle(juce::Rectangle<float>(std::min(correlationTrack_.getCentreX(), correlationEnd),
                                                  correlationTrack_.getY(),
                                                  std::abs(correlationEnd - correlationTrack_.getCentreX()),
                                                  correlationTrack_.getHeight()),
                           3.0f);

    if (!masterArea_.isEmpty()) {
        const auto masterStrip = masterArea_.toFloat();
        g.setColour(kPanel.withAlpha(0.78f));
        g.fillRoundedRectangle(masterStrip, 5.0f);
        g.setColour(kPanelEdge.withAlpha(0.9f));
        g.drawRoundedRectangle(masterStrip, 5.0f, 1.0f);
        g.setColour(kAccentMint.withAlpha(0.58f));
        g.fillRoundedRectangle(masterStrip.withHeight(2.0f).reduced(1.0f, 0.0f), 1.0f);
    }

    g.setFont(juce::FontOptions(11.0f));
    g.setColour(kTextDim);
    const juce::String slopeText = processor_.currentLr8Slope() ? "8th-order Linkwitz-Riley (48 dB)" : "4th-order Linkwitz-Riley";
    const juce::String topologyText = processor_.currentElliptical() ? "elliptical zero-phase Mid" : "independent bass and upper width";
    g.drawText(slopeText + "  |  " + topologyText + "  |  Velvet stereoize",
               footerArea_, juce::Justification::centred);
}

void BassSpaceEditor::resized() {
    if (getWidth() <= 0 || getHeight() <= 0) {
        return;
    }
    if (getWidth() < BassSpaceProcessor::minEditorWidth || getHeight() < BassSpaceProcessor::minEditorHeight) {
        setSize(juce::jmax(getWidth(), BassSpaceProcessor::minEditorWidth),
                juce::jmax(getHeight(), BassSpaceProcessor::minEditorHeight));
        return;
    }

    processor_.setEditorSize(getWidth(), getHeight());

    auto area = getLocalBounds().reduced(12, 8);
    headerArea_ = area.removeFromTop(32);
    badgeArea_ = headerArea_.withSizeKeepingCentre(juce::jmin(210, headerArea_.getWidth() / 3), 20);
    area.removeFromTop(6);
    suiteStrip_.setBounds(area.removeFromTop(34));
    area.removeFromTop(6);
    footerArea_ = area.removeFromBottom(18);
    area.removeFromTop(2);

    const int side = juce::jlimit(148, 230, area.getWidth() / 5);
    auto left = area.removeFromLeft(side);
    auto right = area.removeFromRight(side);
    auto center = area.reduced(6, 0);

    placeKnob(left.removeFromTop(left.getHeight() / 2), freqLabel_, freqSlider_, freqValueLabel_);
    placeKnob(left, stereoizeLabel_, stereoizeSlider_, stereoizeValueLabel_);
    placeKnob(right.removeFromTop(right.getHeight() / 2), widthLabel_, widthSlider_, widthValueLabel_);
    placeKnob(right, upperWidthLabel_, upperWidthSlider_, upperWidthValueLabel_);

    const int masterH = juce::jlimit(118, 176, center.getHeight() * 2 / 5);
    masterArea_ = center.removeFromBottom(masterH);
    fieldArea_ = center;

    auto field = fieldArea_;
    auto correlationRow = field.removeFromBottom(juce::jmax(52, fieldArea_.getHeight() / 5));
    correlationTrack_ = juce::Rectangle<float>(static_cast<float>(fieldArea_.getX() + 18),
                                               static_cast<float>(correlationRow.getY() + 2),
                                               static_cast<float>(juce::jmax(40, fieldArea_.getWidth() - 36)),
                                               8.0f);
    auto textRow = correlationRow.withTrimmedTop(12);
    correlationLabel_.setBounds(textRow.removeFromLeft(juce::jmax(108, textRow.getWidth() / 3)));
    correlationValueLabel_.setBounds(textRow);
    const float readout = juce::jlimit(26.0f, 52.0f, static_cast<float>(correlationValueLabel_.getHeight()) - 4.0f);
    correlationValueLabel_.setFont(juce::FontOptions(readout, juce::Font::bold));

    field.removeFromBottom(6);
    ringArea_ = field.reduced(4, 2);
    stereoFieldAnchor_.setBounds(ringArea_);

    auto cells = masterArea_.reduced(4, 6);
    const int cellW = cells.getWidth() / 4;
    placeKnob(cells.removeFromLeft(cellW), subDriveLabel_, subDriveSlider_, subDriveValueLabel_);
    placeKnob(cells.removeFromLeft(cellW), outputDbLabel_, outputDbSlider_, outputDbValueLabel_);
    placeKnob(cells.removeFromLeft(cellW), upperTiltLabel_, upperTiltSlider_, upperTiltValueLabel_);
    const int buttonWidth = juce::jmin(124, juce::jmax(72, cells.getWidth() - 8));
    const int buttonHeight = juce::jlimit(24, 30, (cells.getHeight() - 10) / 3);
    const int totalStackHeight = buttonHeight * 3 + 8;
    auto buttonColumn = cells.withSizeKeepingCentre(buttonWidth, totalStackHeight);
    soloLowButton_.setBounds(buttonColumn.removeFromTop(buttonHeight));
    buttonColumn.removeFromTop(4);
    slopeButton_.setBounds(buttonColumn.removeFromTop(buttonHeight));
    buttonColumn.removeFromTop(4);
    ellipticalButton_.setBounds(buttonColumn.removeFromTop(buttonHeight));

    if (presetBrowser_.isVisible()) {
        positionPresetBrowser();
    }
    updateValueLabels();
}

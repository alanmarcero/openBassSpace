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
constexpr int kNameHeight = 16;
constexpr int kValueHeight = 20;
constexpr int kKnobMargin = 6;

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

juce::Colour widthColour(float percent) {
    if (percent < 10.0f) {
        return kAccentMint;
    }
    if (percent > 105.0f) {
        return kAccentGold;
    }
    return kTextWhite;
}

void configureKnob(juce::Slider& slider, juce::Colour fill, double doubleClickValue,
                   const juce::String& title, const juce::String& description, const juce::String& tooltip) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 96, kValueHeight);
    slider.setColour(juce::Slider::textBoxTextColourId, fill);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::rotarySliderFillColourId, fill);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    slider.setColour(juce::Slider::thumbColourId, kTextWhite);
    slider.setDoubleClickReturnValue(true, doubleClickValue);
    slider.setTitle(title);
    slider.setDescription(description);
    slider.setTooltip(tooltip);
    slider.setWantsKeyboardFocus(true);
    slider.setMouseClickGrabsKeyboardFocus(false);
}

void configureName(juce::Label& label, const juce::String& text) {
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, kTextWhite);
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
    button.setMouseClickGrabsKeyboardFocus(false);
}

void placeKnob(juce::Rectangle<int> cell, juce::Label& name, juce::Slider& slider) {
    const int dial = juce::jlimit(48, cell.getWidth(), cell.getHeight() - kNameHeight - kValueHeight - 2 * kKnobMargin);
    auto group = cell.withSizeKeepingCentre(cell.getWidth(), kNameHeight + dial + kValueHeight);
    name.setBounds(group.removeFromTop(kNameHeight));
    slider.setBounds(group.withSizeKeepingCentre(dial, group.getHeight()));
}

} // namespace

juce::Label* BassSpaceTheme::createSliderTextBox(juce::Slider& slider) {
    auto* box = juce::LookAndFeel_V4::createSliderTextBox(slider);
    box->setFont(juce::FontOptions(15.0f, juce::Font::bold));
    box->setColour(juce::TextEditor::backgroundColourId, kBackground);
    box->setColour(juce::TextEditor::focusedOutlineColourId, kAccentMint);
    return box;
}

void BassSpaceTheme::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                                      float startAngle, float endAngle, juce::Slider& slider) {
    juce::LookAndFeel_V4::drawRotarySlider(g, x, y, width, height, position, startAngle, endAngle, slider);
    if (slider.hasKeyboardFocus(false)) {
        g.setColour(kAccentMint);
        g.drawRoundedRectangle(juce::Rectangle<int>(x, y, width, height).toFloat().reduced(1.0f), 6.0f, 1.5f);
    }
}

void BassSpaceTheme::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& background,
                                          bool highlighted, bool down) {
    juce::LookAndFeel_V4::drawButtonBackground(g, button, background, highlighted, down);
    if (button.hasKeyboardFocus(false)) {
        g.setColour(kAccentMint);
        g.drawRoundedRectangle(button.getLocalBounds().toFloat().reduced(1.5f), 6.0f, 2.0f);
    }
}

void FineSlider::mouseDown(const juce::MouseEvent& e) {
    lastMouse_ = e.position;
    dragPosition_ = e.position;
    juce::Slider::mouseDown(e);
}

// Shift scales each mouse step, so pressing or releasing Shift mid-drag does not jump the value.
void FineSlider::mouseDrag(const juce::MouseEvent& e) {
    const float speed = e.mods.isShiftDown() ? 0.1f : 1.0f;
    dragPosition_ += (e.position - lastMouse_) * speed;
    lastMouse_ = e.position;
    juce::Slider::mouseDrag(e.withNewPosition(dragPosition_));
}

// Each arrow-key step is one gesture, so the host and the undo history record it.
bool FineSlider::keyPressed(const juce::KeyPress& key) {
    if (key.getModifiers().isAnyModifierKeyDown()) {
        return false;
    }
    const ScopedDragNotification gesture(*this);
    return juce::Slider::keyPressed(key);
}

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
    faceplate_ = juce::ImageCache::getFromMemory(
        BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize);
    stereoField_ = juce::Drawable::createFromImageData(BinaryData::stereofield_svg, BinaryData::stereofield_svgSize);

    configureKnob(freqSlider_, kAccentMint, 120.0, "Crossover frequency",
                  "Sets the boundary between the bass and upper stereo width controls.",
                  "Set the boundary between Bass width and Upper width.");
    configureName(freqLabel_, "Crossover");

    configureKnob(widthSlider_, kAccentGold, 0.0, "Bass stereo width",
                  "Sets the stereo width below the crossover frequency.",
                  "Set the bass stereo width below the crossover frequency.");
    configureName(widthLabel_, "Bass width");

    configureKnob(upperWidthSlider_, kAccentGold, 100.0, "Upper-band width",
                  "Sets stereo width above the crossover frequency.",
                  "Set stereo width above the crossover frequency.");
    configureName(upperWidthLabel_, "Upper width");

    configureKnob(stereoizeSlider_, kAccentMint, 0.0, "Stereoize",
                  "Adds a mono-compatible decorrelated side signal above the crossover.",
                  "Add a decorrelated side signal above the crossover. The bass remains centered.");
    configureName(stereoizeLabel_, "Stereoize");

    correlationLabel_.setText("Correlation", juce::dontSendNotification);
    correlationValueLabel_.setComponentID("correlationValue");
    bassCorrelationValueLabel_.setComponentID("bassCorrelationValue");
    for (auto* name : { &correlationLabel_, &bassCorrelationLabel_ }) {
        name->setJustificationType(juce::Justification::centredLeft);
        name->setFont(juce::FontOptions(13.0f, juce::Font::bold));
        name->setColour(juce::Label::textColourId, kTextDim);
    }
    for (auto* value : { &correlationValueLabel_, &bassCorrelationValueLabel_ }) {
        value->setJustificationType(juce::Justification::centredRight);
        value->setFont(juce::FontOptions(18.0f, juce::Font::bold));
    }

    configureKnob(outputDbSlider_, kAccentMint, 0.0, "Output level",
                  "Sets the plug-in output level in decibels.",
                  "Set the plug-in output level in decibels.");
    configureName(outputDbLabel_, "Output dB");

    configureKnob(subDriveSlider_, kAccentMint, 0.0, "Sub harmonic drive",
                  "Adds mono-locked sub-octave and harmonic warmth to the bass band.",
                  "Blend mono-locked sub-octave synthesis and ADAA saturation into the bass band.");
    configureName(subDriveLabel_, "Sub drive");

    configureKnob(upperTiltSlider_, kAccentGold, 0.0, "Upper air tilt",
                  "Tilts the upper stereo band brightness from -6 dB to +6 dB.",
                  "Tilt upper-band high frequencies (-6 dB dark to +6 dB air).");
    configureName(upperTiltLabel_, "Upper tilt");

    configureModeButton(soloLowButton_, kAccentMint, "Solo bass band",
                        "Auditions only the low-frequency band below the crossover.",
                        "Audition the isolated bass band below the crossover frequency.");
    configureModeButton(slopeButton_, kAccentGold, "Slope",
                        "Switches the crossover slope between 24 dB and 48 dB per octave.",
                        "Switch the crossover slope between 24 dB and 48 dB per octave. 48 dB separates the bands more sharply.");
    configureModeButton(ellipticalButton_, kAccentMint, "Side-only split",
                        "Splits only the Side at the crossover and passes the Mid unfiltered.",
                        "Split only the Side at the crossover. The Mid passes unfiltered and keeps its phase "
                        "while Sub drive and Upper tilt are at 0.");
    slopeButton_.setComponentID("slopeButton");
    ellipticalButton_.setComponentID("sideOnlyButton");

    for (auto* component : std::initializer_list<juce::Component*>{
             &freqSlider_, &freqLabel_,
             &widthSlider_, &widthLabel_,
             &upperWidthSlider_, &upperWidthLabel_,
             &stereoizeSlider_, &stereoizeLabel_,
             &correlationLabel_, &correlationValueLabel_, &bassCorrelationLabel_, &bassCorrelationValueLabel_,
             &outputDbSlider_, &outputDbLabel_,
             &subDriveSlider_, &subDriveLabel_,
             &upperTiltSlider_, &upperTiltLabel_,
             &soloLowButton_, &slopeButton_, &ellipticalButton_, &suiteStrip_ }) {
        addAndMakeVisible(component);
    }

    stereoFieldAnchor_.setComponentID("stereoField");
    stereoFieldAnchor_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(stereoFieldAnchor_);
    addChildComponent(presetBrowser_);
    setLookAndFeel(&theme_);

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
    slopeButton_.onStateChange = [this] {
        slopeButton_.setButtonText("SLOPE " + processor_.parameters.getParameter(bass_space::params::kSlope)->getCurrentValueAsText());
    };
    slopeButton_.onStateChange();
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
    updateMeter();
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
    updateMeter();
    suiteStrip_.refresh();
    if (!ringArea_.isEmpty()) {
        repaint(ringArea_.getUnion(meterArea_).getUnion(footerArea_));
    }
}

void BassSpaceEditor::updateMeter() {
    const std::array<float, 2> readings { processor_.currentCorrelation(), processor_.currentBassCorrelation() };
    const std::array<juce::Label*, 2> values { &correlationValueLabel_, &bassCorrelationValueLabel_ };
    for (size_t row = 0; row < values.size(); ++row) {
        const float hundredths = std::round(readings[row] * 100.0f) / 100.0f;
        const auto text = [hundredths]() -> juce::String {
            if (std::isnan(hundredths)) {
                return "--";
            }
            if (std::abs(hundredths) < 0.005f) {
                return "0.00";
            }
            return (hundredths > 0.0f ? "+" : "") + juce::String(hundredths, 2);
        }();
        const auto colour = [hundredths] {
            if (std::isnan(hundredths)) {
                return kTextDim;
            }
            return hundredths < -0.005f ? kAccentGold : kAccentMint;
        }();
        values[row]->setText(text, juce::dontSendNotification);
        values[row]->setColour(juce::Label::textColourId, colour);
    }
    const auto crossover = processor_.parameters.getParameter(bass_space::params::kFrequency)->getCurrentValueAsText();
    bassCorrelationLabel_.setText("Below " + crossover, juce::dontSendNotification);
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
    g.setColour(kPanelRule);
    for (const auto& divider : knobDividers_) {
        g.drawLine(divider, 1.0f);
    }

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

    // Long thin wings show the upper band. Short thick wings show the bass band.
    const float bassWidth = processor_.currentWidth();
    const float upperWidth = processor_.currentUpperWidth();
    const bool soloBass = processor_.currentSoloLow();
    const float wingLength = juce::jlimit(28.0f, 64.0f, static_cast<float>(ringArea_.getHeight()) * 0.28f);
    struct Wings { float width; float length; float thickness; juce::Colour colour; };
    const std::array<Wings, 2> bands {{
        { upperWidth, wingLength, 2.5f, widthColour(upperWidth).withAlpha(soloBass ? 0.25f : 0.9f) },
        { bassWidth, wingLength * 0.55f, 5.0f, widthColour(bassWidth) },
    }};
    for (const auto& band : bands) {
        const float spread = band.width / 200.0f * std::numbers::pi_v<float> * 0.42f;
        const juce::Point<float> tip(std::sin(spread) * band.length, -std::cos(spread) * band.length);
        juce::Path wings;
        wings.startNewSubPath(arcCentre.translated(-tip.x, tip.y));
        wings.lineTo(arcCentre);
        wings.lineTo(arcCentre + tip);
        g.setColour(band.colour);
        g.strokePath(wings, juce::PathStrokeType(band.thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour(widthColour(bassWidth));
    g.fillEllipse(arcCentre.x - 3.5f, arcCentre.y - 3.5f, 7.0f, 7.0f);

    const auto widthCaption = [](const juce::RangedAudioParameter& width) {
        if (width.getValue() <= 0.0f) {
            return juce::String("MONO");
        }
        return width.getCurrentValueAsText();
    };
    const auto& parameters = processor_.parameters;
    const auto upperCaption = [&]() -> juce::String {
        if (soloBass) {
            return "UPPER MUTED";
        }
        const auto upper = "UPPER " + widthCaption(*parameters.getParameter(bass_space::params::kUpperWidth));
        if (processor_.currentStereoize() < 0.5f) {
            return upper;
        }
        return upper + " + STEREOIZE " + parameters.getParameter(bass_space::params::kStereoize)->getCurrentValueAsText();
    }();
    const juce::Font captionFont(juce::FontOptions(10.5f, juce::Font::bold));
    juce::AttributedString caption;
    caption.setJustification(juce::Justification::centred);
    caption.append("BASS " + widthCaption(*parameters.getParameter(bass_space::params::kWidth)) + " below "
                       + parameters.getParameter(bass_space::params::kFrequency)->getCurrentValueAsText(),
                   captionFont, widthColour(bassWidth));
    caption.append("  |  ", captionFont, kTextDim);
    caption.append(upperCaption, captionFont, widthColour(upperWidth).withAlpha(soloBass ? 0.6f : 1.0f));
    caption.draw(g, ringArea_.toFloat().removeFromBottom(16.0f));

    const std::array<float, 2> readings { processor_.currentCorrelation(), processor_.currentBassCorrelation() };
    for (size_t row = 0; row < correlationTracks_.size(); ++row) {
        const auto& track = correlationTracks_[row];
        g.setColour(kPanelEdge);
        g.fillRoundedRectangle(track, 3.0f);
        if (!std::isnan(readings[row])) {
            const float reading = juce::jlimit(-1.0f, 1.0f, readings[row]);
            const float end = track.getCentreX() + reading * track.getWidth() * 0.5f;
            g.setColour(reading < 0.0f ? kAccentGold : kAccentMint);
            g.fillRoundedRectangle(track.withX(std::min(track.getCentreX(), end)).withWidth(std::abs(end - track.getCentreX())),
                                   3.0f);
        }
        g.setColour(kTextDim);
        for (const float mark : { -1.0f, 0.0f, 1.0f }) {
            g.fillRect(juce::Rectangle<float>(1.0f, track.getHeight() + 6.0f)
                           .withCentre({ track.getCentreX() + mark * track.getWidth() * 0.5f, track.getCentreY() }));
        }
    }
    const auto& lowerTrack = correlationTracks_[1];
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    for (const auto& [mark, text] : { std::pair{ -1.0f, "-1" }, std::pair{ 0.0f, "0" }, std::pair{ 1.0f, "+1" } }) {
        const juce::Point<float> under(lowerTrack.getCentreX() + mark * lowerTrack.getWidth() * 0.5f, lowerTrack.getBottom() + 12.0f);
        g.drawText(text, juce::Rectangle<float>(24.0f, 12.0f).withCentre(under), juce::Justification::centred);
    }

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
    const auto slope = processor_.parameters.getParameter(bass_space::params::kSlope)->getCurrentValueAsText();
    const juce::String split = processor_.currentElliptical() ? "  |  Side-only split" : "";
    g.drawText("Slope " + slope + split + "  |  Velvet stereoize",
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
    const auto divider = [](juce::Rectangle<int> column) {
        return juce::Line<int>(column.getX() + 16, column.getCentreY(), column.getRight() - 16, column.getCentreY()).toFloat();
    };
    knobDividers_ = { divider(left), divider(right) };

    placeKnob(left.removeFromTop(left.getHeight() / 2), freqLabel_, freqSlider_);
    placeKnob(left, stereoizeLabel_, stereoizeSlider_);
    placeKnob(right.removeFromTop(right.getHeight() / 2), widthLabel_, widthSlider_);
    placeKnob(right, upperWidthLabel_, upperWidthSlider_);

    const int masterH = juce::jlimit(118, 176, center.getHeight() * 2 / 5);
    masterArea_ = center.removeFromBottom(masterH);
    fieldArea_ = center;

    auto field = fieldArea_;
    meterArea_ = field.removeFromBottom(72).reduced(12, 6);
    auto meter = meterArea_;
    const auto placeMeterRow = [&meter](juce::Label& name, juce::Label& value) {
        auto row = meter.removeFromTop(22);
        name.setBounds(row.removeFromLeft(104));
        value.setBounds(row.removeFromRight(60));
        return row.reduced(10, 7).toFloat();
    };
    correlationTracks_ = { placeMeterRow(correlationLabel_, correlationValueLabel_),
                           placeMeterRow(bassCorrelationLabel_, bassCorrelationValueLabel_) };

    field.removeFromBottom(6);
    ringArea_ = field.reduced(4, 2);
    stereoFieldAnchor_.setBounds(ringArea_);

    auto cells = masterArea_.reduced(4, 6);
    const int cellW = cells.getWidth() / 4;
    placeKnob(cells.removeFromLeft(cellW), subDriveLabel_, subDriveSlider_);
    placeKnob(cells.removeFromLeft(cellW), outputDbLabel_, outputDbSlider_);
    placeKnob(cells.removeFromLeft(cellW), upperTiltLabel_, upperTiltSlider_);
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
}

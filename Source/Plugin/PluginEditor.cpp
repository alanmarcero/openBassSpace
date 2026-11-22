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
constexpr int kButtonHeight = 36;

juce::Colour widthColour(float percent) {
    if (percent < 10.0f) {
        return kAccentMint;
    }
    if (percent > 105.0f) {
        return kAccentGold;
    }
    return kTextWhite;
}

void configureKnob(juce::Slider& slider, juce::Label& name, const juce::String& title, juce::Colour fill,
                   const juce::String& tooltip) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 96, kValueHeight);
    slider.setColour(juce::Slider::textBoxTextColourId, fill);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::rotarySliderFillColourId, fill);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22323c));
    slider.setColour(juce::Slider::thumbColourId, kTextWhite);
    slider.setTitle(title);
    slider.setDescription(tooltip);
    slider.setTooltip(tooltip);
    slider.setWantsKeyboardFocus(true);
    slider.setMouseClickGrabsKeyboardFocus(false);
    name.setText(title, juce::dontSendNotification);
    name.setJustificationType(juce::Justification::centred);
    name.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    name.setColour(juce::Label::textColourId, kTextWhite);
}

void configureModeButton(juce::TextButton& button, juce::Colour activeColour, const juce::String& title,
                         const juce::String& tooltip) {
    button.setClickingTogglesState(true);
    button.setColour(juce::TextButton::buttonColourId, kPanel);
    button.setColour(juce::TextButton::buttonOnColourId, activeColour);
    button.setColour(juce::TextButton::textColourOffId, kTextWhite);
    button.setColour(juce::TextButton::textColourOnId, kBackground);
    button.setTitle(title);
    button.setDescription(tooltip);
    button.setTooltip(tooltip);
    button.setMouseClickGrabsKeyboardFocus(false);
}

void placeKnob(juce::Rectangle<int> cell, juce::Label& name, juce::Slider& slider) {
    const int dial = juce::jlimit(48, cell.getWidth(), cell.getHeight() - kNameHeight - kValueHeight - 2 * kKnobMargin);
    auto group = cell.withSizeKeepingCentre(cell.getWidth(), kNameHeight + dial + kValueHeight);
    name.setBounds(group.removeFromTop(kNameHeight));
    slider.setBounds(group.withSizeKeepingCentre(dial, group.getHeight()));
}

void drawPadlock(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour) {
    const float bodyHeight = bounds.getHeight() * 0.56f;
    const auto body = bounds.withTop(bounds.getBottom() - bodyHeight);
    const auto shackle = juce::Rectangle<float>(bounds.getWidth() * 0.64f, bounds.getHeight() * 0.54f)
                             .withCentre({ bounds.getCentreX(), body.getY() });

    juce::Path shacklePath;
    shacklePath.addArc(shackle.getX(), shackle.getY(), shackle.getWidth(), shackle.getHeight(),
                       -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
    g.setColour(colour);
    g.strokePath(shacklePath, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillRoundedRectangle(body, 1.5f);
}

} // namespace

juce::Font BassSpaceTheme::getTextButtonFont(juce::TextButton&, int) {
    return withDefaultMetrics(juce::FontOptions(14.0f));
}

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
    if (static_cast<bool>(slider.getProperties().getWithDefault("paramLocked", false))) {
        drawPadlock(g, { static_cast<float>(x + width) - 13.0f, static_cast<float>(y) + 2.0f, 10.0f, 12.0f }, kAccentGold);
    }
    if (slider.hasKeyboardFocus(false)) {
        g.setColour(kAccentMint);
        g.drawRoundedRectangle(juce::Rectangle<int>(x, y, width, height).toFloat().reduced(1.0f), 6.0f, 1.5f);
    }
}

void BassSpaceTheme::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& background,
                                          bool highlighted, bool down) {
    juce::LookAndFeel_V4::drawButtonBackground(g, button, background, highlighted, down);
    if (static_cast<bool>(button.getProperties().getWithDefault("paramLocked", false))) {
        const auto bounds = button.getLocalBounds().toFloat();
        const auto colour = button.getToggleState() ? kBackground : kAccentGold;
        drawPadlock(g, { bounds.getRight() - 13.0f, bounds.getY() + 3.0f, 9.0f, 11.0f }, colour);
    }
    if (button.hasKeyboardFocus(false)) {
        g.setColour(kAccentMint);
        g.drawRoundedRectangle(button.getLocalBounds().toFloat().reduced(1.5f), 6.0f, 2.0f);
    }
}

void FineSlider::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) {
        if (onPopupMenu) {
            onPopupMenu();
        }
        return;
    }
    if (e.eventComponent != this) {
        return;
    }
    lastMouse_ = e.position;
    dragPosition_ = e.position;
    juce::Slider::mouseDown(e);
}

// Shift scales each mouse step, so a Shift change mid-drag does not jump the value.
void FineSlider::mouseDrag(const juce::MouseEvent& e) {
    const float speed = e.mods.isShiftDown() ? 0.1f : 1.0f;
    dragPosition_ += (e.position - lastMouse_) * speed;
    lastMouse_ = e.position;
    juce::Slider::mouseDrag(e.withNewPosition(dragPosition_));
}

// Arrow keys step 1% of the range, or one interval with Shift. Page Up and Page Down step 10%.
// Only these keys open a gesture, so other keys reach the editor and the host sees no empty gesture.
bool FineSlider::keyPressed(const juce::KeyPress& key) {
    const auto modifiers = key.getModifiers();
    const bool arrowUp = key.isKeyCode(juce::KeyPress::upKey) || key.isKeyCode(juce::KeyPress::rightKey);
    const bool arrowDown = key.isKeyCode(juce::KeyPress::downKey) || key.isKeyCode(juce::KeyPress::leftKey);
    const bool pageUp = key.isKeyCode(juce::KeyPress::pageUpKey);
    const bool pageDown = key.isKeyCode(juce::KeyPress::pageDownKey);
    if (modifiers.isCommandDown() || modifiers.isCtrlDown() || modifiers.isAltDown()
        || !(arrowUp || arrowDown || pageUp || pageDown)) {
        return false;
    }

    const bool arrow = arrowUp || arrowDown;
    const double interval = getInterval();
    const double rangeStep = (getMaximum() - getMinimum()) * (arrow ? 0.01 : 0.1);
    const double step = arrow && modifiers.isShiftDown() ? interval : rangeStep;
    const double direction = arrowUp || pageUp ? 1.0 : -1.0;
    const ScopedDragNotification gesture(*this);
    setValue(getValue() + direction * step, juce::sendNotificationSync);
    return true;
}

// The value box copies the tooltip only when it is created, so later tooltips go to it too.
void FineSlider::setTooltip(const juce::String& newTooltip) {
    juce::Slider::setTooltip(newTooltip);
    for (auto* child : getChildren()) {
        if (auto* valueBox = dynamic_cast<juce::Label*>(child)) {
            valueBox->setTooltip(newTooltip);
        }
    }
}

void FineSlider::childrenChanged() {
    juce::Slider::childrenChanged();
    for (auto* child : getChildren()) {
        if (auto* valueBox = dynamic_cast<juce::Label*>(child)) {
            valueBox->removeMouseListener(this);
            valueBox->addMouseListener(this, false);
        }
    }
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

    configureKnob(freqSlider_, freqLabel_, "Crossover", kAccentMint,
                  "Set the boundary between Bass width and Upper width.");
    configureKnob(widthSlider_, widthLabel_, "Bass width", kAccentGold,
                  "Set the bass stereo width below the crossover frequency.");
    configureKnob(upperWidthSlider_, upperWidthLabel_, "Upper width", kAccentGold,
                  "Set stereo width above the crossover frequency.");
    configureKnob(stereoizeSlider_, stereoizeLabel_, "Stereoize", kAccentMint,
                  "Add a decorrelated side signal above the crossover. The bass remains centered.");

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

    configureKnob(outputDbSlider_, outputDbLabel_, "Output dB", kAccentMint,
                  "Set the plug-in output level in decibels.");
    configureKnob(subDriveSlider_, subDriveLabel_, "Sub drive", kAccentMint,
                  "Blend mono-locked sub-octave synthesis and ADAA saturation into the bass band.");
    configureKnob(upperTiltSlider_, upperTiltLabel_, "Upper tilt", kAccentGold,
                  "Tilt upper-band high frequencies (-6 dB dark to +6 dB air).");

    configureModeButton(soloLowButton_, kAccentMint, "Solo bass",
                        "Audition the isolated bass band below the crossover frequency.");
    configureModeButton(slopeButton_, kAccentGold, "Slope 24 dB",
                        "Switch the crossover slope between 24 dB and 48 dB per octave. 48 dB separates the bands more sharply.");
    configureModeButton(ellipticalButton_, kAccentMint, "Side-only split",
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
        const auto slope = processor_.parameters.getParameter(bass_space::params::kSlope)->getCurrentValueAsText();
        slopeButton_.setButtonText("SLOPE " + slope);
        slopeButton_.setTitle("Slope " + slope);
    };
    slopeButton_.onStateChange();
    ellipticalAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.parameters, bass_space::params::kElliptical, ellipticalButton_);

    const auto bindSliderMenu = [this](FineSlider& slider, const char* paramId) {
        slider.onPopupMenu = [this, &slider, paramId] { showParamLockMenu(paramId, slider); };
    };
    bindSliderMenu(freqSlider_, bass_space::params::kFrequency);
    bindSliderMenu(widthSlider_, bass_space::params::kWidth);
    bindSliderMenu(upperWidthSlider_, bass_space::params::kUpperWidth);
    bindSliderMenu(stereoizeSlider_, bass_space::params::kStereoize);
    bindSliderMenu(outputDbSlider_, bass_space::params::kOutputDb);
    bindSliderMenu(subDriveSlider_, bass_space::params::kSubDrive);
    bindSliderMenu(upperTiltSlider_, bass_space::params::kUpperTilt);

    const auto bindButtonMenu = [this](alanmarcero::suite::PopupTextButton& button, const char* paramId) {
        button.onPopupMenu = [this, &button, paramId] { showParamLockMenu(paramId, button); };
    };
    bindButtonMenu(soloLowButton_, bass_space::params::kSoloLow);
    bindButtonMenu(slopeButton_, bass_space::params::kSlope);
    bindButtonMenu(ellipticalButton_, bass_space::params::kElliptical);

    presetBrowser_.onChanged = [this] { suiteStrip_.refresh(); };
    presetBrowser_.onSaved = [this](const juce::String& name) { processor_.markPresetSaved(name); };
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
    actions.presetName = [this] { return processor_.currentPresetName() + (processor_.isPresetEdited() ? " *" : ""); };
    actions.previousPreset = [this] { processor_.stepPreset(-1); };
    actions.nextPreset = [this] { processor_.stepPreset(1); };
    actions.isBypassLocked = [this] { return processor_.isParamLocked(bass_space::params::kBypass); };
    actions.onBypassPopupMenu = [this](juce::Component& target) {
        showParamLockMenu(bass_space::params::kBypass, target);
    };
    actions.togglePresetBrowser = [this] {
        presetBrowser_.setVisible(!presetBrowser_.isVisible());
        if (presetBrowser_.isVisible()) {
            processor_.presetLibrary().rescan();
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
    refreshDisplay();
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

void BassSpaceEditor::refreshDisplay() {
    updateMeter();
    suiteStrip_.refresh();
    syncLockIndicators();

    // Solo bass mutes the upper band, so its dials draw dim and their tooltips say how to hear them.
    const bool soloBass = processor_.currentSoloLow();
    if (soloBass != upperBandDimmed_) {
        upperBandDimmed_ = soloBass;
        const juce::String soloHint = soloBass ? " Turn off Solo bass to hear it." : "";
        for (auto* dial : { &upperWidthSlider_, &stereoizeSlider_, &upperTiltSlider_ }) {
            dial->setAlpha(soloBass ? 0.4f : 1.0f);
            dial->setTooltip(dial->getDescription() + soloHint);
        }
        for (auto* name : { &upperWidthLabel_, &stereoizeLabel_, &upperTiltLabel_ }) {
            name->setAlpha(soloBass ? 0.4f : 1.0f);
        }
    }

    const std::array<float, 6> field { processor_.artworkPhase(), processor_.currentWidth(), processor_.currentUpperWidth(),
                                       processor_.currentFrequency(), processor_.currentStereoize(),
                                       soloBass ? 1.0f : 0.0f };
    if (field != lastField_) {
        lastField_ = field;
        repaint(ringArea_);
    }

    const auto slope = processor_.parameters.getParameter(bass_space::params::kSlope)->getCurrentValueAsText();
    const juce::String split = processor_.currentElliptical() ? "  |  Side-only split" : "";
    const auto footer = "Slope " + slope + split + "  |  Velvet stereoize";
    if (footer != footerText_) {
        footerText_ = footer;
        repaint(footerArea_);
    }
}

void BassSpaceEditor::updateMeter() {
    const std::array<std::pair<juce::Label*, float>, 2> rows {{
        { &correlationValueLabel_, processor_.currentCorrelation() },
        { &bassCorrelationValueLabel_, processor_.currentBassCorrelation() },
    }};
    for (const auto& [value, reading] : rows) {
        const float hundredths = std::round(reading * 100.0f) / 100.0f;
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
        if (text != value->getText()) {
            repaint(meterArea_);
        }
        value->setText(text, juce::dontSendNotification);
        value->setColour(juce::Label::textColourId, colour);
    }
    const auto crossover = processor_.parameters.getParameter(bass_space::params::kFrequency)->getCurrentValueAsText();
    bassCorrelationLabel_.setText("Below " + crossover, juce::dontSendNotification);
}

void BassSpaceEditor::showParamLockMenu(const juce::String& paramId, juce::Component& target) {
    auto* param = processor_.parameters.getParameter(paramId);
    if (param == nullptr) {
        return;
    }

    const bool locked = processor_.isParamLocked(paramId);
    juce::PopupMenu menu;
    menu.setLookAndFeel(&theme_);
    menu.addItem(1, (locked ? "Unlock " : "Lock ") + param->getName(64));
    if (processor_.hasAnyLockedParams()) {
        menu.addSeparator();
        menu.addItem(2, "Unlock all parameters");
    }

    juce::Component::SafePointer<BassSpaceEditor> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target),
                       [safeThis, paramId](int result) {
                           if (safeThis == nullptr) {
                               return;
                           }
                           if (result == 1) {
                               safeThis->processor_.setParamLocked(paramId, !safeThis->processor_.isParamLocked(paramId));
                               safeThis->refreshDisplay();
                           } else if (result == 2) {
                               safeThis->processor_.unlockAllParams();
                               safeThis->refreshDisplay();
                           }
                       });
}

void BassSpaceEditor::syncLockIndicators() {
    const std::pair<const char*, juce::Component*> controls[] = {
        { bass_space::params::kFrequency, &freqSlider_ },
        { bass_space::params::kWidth, &widthSlider_ },
        { bass_space::params::kUpperWidth, &upperWidthSlider_ },
        { bass_space::params::kStereoize, &stereoizeSlider_ },
        { bass_space::params::kOutputDb, &outputDbSlider_ },
        { bass_space::params::kSubDrive, &subDriveSlider_ },
        { bass_space::params::kUpperTilt, &upperTiltSlider_ },
        { bass_space::params::kSoloLow, &soloLowButton_ },
        { bass_space::params::kSlope, &slopeButton_ },
        { bass_space::params::kElliptical, &ellipticalButton_ },
    };

    for (const auto& [paramId, control] : controls) {
        const bool locked = processor_.isParamLocked(paramId);
        if (static_cast<bool>(control->getProperties().getWithDefault("paramLocked", false)) != locked) {
            control->getProperties().set("paramLocked", locked);
            control->repaint();
        }
    }
}

void BassSpaceEditor::positionPresetBrowser() {
    auto anchor = getLocalArea(&suiteStrip_, suiteStrip_.presetButtonBounds());
    const int browserWidth = juce::jmin(400, getWidth() - 24);
    const int x = juce::jlimit(8, juce::jmax(8, getWidth() - browserWidth - 8), anchor.getRight() - browserWidth);
    const int height = juce::jmin(320, juce::jmax(220, getHeight() - anchor.getBottom() - 16));
    presetBrowser_.setBounds(x, anchor.getBottom() + 4, browserWidth, height);
}

void BassSpaceEditor::paintBackground(juce::Graphics& g) const {
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

    const auto centerArea = fieldArea_.toFloat();
    g.setColour(kPanel);
    g.fillRoundedRectangle(centerArea, 5.0f);
    g.setColour(kPanelEdge);
    g.drawRoundedRectangle(centerArea, 5.0f, 1.0f);

    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(kTextDim);
    g.drawText("STEREO FIELD", ringArea_.withHeight(16), juce::Justification::centred);

    if (!masterArea_.isEmpty()) {
        const auto masterStrip = masterArea_.toFloat();
        g.setColour(kPanel.withAlpha(0.78f));
        g.fillRoundedRectangle(masterStrip, 5.0f);
        g.setColour(kPanelEdge.withAlpha(0.9f));
        g.drawRoundedRectangle(masterStrip, 5.0f, 1.0f);
        g.setColour(kAccentMint.withAlpha(0.58f));
        g.fillRoundedRectangle(masterStrip.withHeight(2.0f).reduced(1.0f, 0.0f), 1.0f);
    }
}

void BassSpaceEditor::paint(juce::Graphics& g) {
    // The faceplate and panels render once per size, at the display's pixel scale.
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int imageWidth = juce::roundToInt(scale * static_cast<float>(getWidth()));
    if (background_.getWidth() != imageWidth) {
        background_ = juce::Image(juce::Image::RGB, imageWidth, juce::roundToInt(scale * static_cast<float>(getHeight())), false);
        juce::Graphics backdrop(background_);
        backdrop.addTransform(juce::AffineTransform::scale(scale));
        paintBackground(backdrop);
    }
    g.drawImage(background_, getLocalBounds().toFloat());

    if (fieldArea_.isEmpty()) {
        return;
    }

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

    const std::array<std::pair<juce::Rectangle<float>, float>, 2> bars {{
        { correlationTracks_[0], processor_.currentCorrelation() },
        { correlationTracks_[1], processor_.currentBassCorrelation() },
    }};
    for (const auto& [track, correlation] : bars) {
        g.setColour(kPanelEdge);
        g.fillRoundedRectangle(track, 3.0f);
        if (!std::isnan(correlation)) {
            const float reading = juce::jlimit(-1.0f, 1.0f, correlation);
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

    g.setFont(juce::FontOptions(11.0f));
    g.setColour(kTextDim);
    g.drawText(footerText_, footerArea_, juce::Justification::centred);
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
    background_ = {};

    auto area = getLocalBounds().reduced(12, 8);
    headerArea_ = area.removeFromTop(32);
    badgeArea_ = headerArea_.withSizeKeepingCentre(juce::jmin(210, headerArea_.getWidth() / 3), 20);
    area.removeFromTop(6);
    suiteStrip_.setBounds(area.removeFromTop(44));
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
    auto buttonColumn = cells.removeFromRight(juce::jmax(148, cells.getWidth() / 4));
    const int cellW = cells.getWidth() / 3;
    placeKnob(cells.removeFromLeft(cellW), subDriveLabel_, subDriveSlider_);
    placeKnob(cells.removeFromLeft(cellW), outputDbLabel_, outputDbSlider_);
    placeKnob(cells, upperTiltLabel_, upperTiltSlider_);
    buttonColumn = buttonColumn.withSizeKeepingCentre(buttonColumn.getWidth() - 8, kButtonHeight * 3 + 12);
    soloLowButton_.setBounds(buttonColumn.removeFromTop(kButtonHeight));
    buttonColumn.removeFromTop(6);
    slopeButton_.setBounds(buttonColumn.removeFromTop(kButtonHeight));
    buttonColumn.removeFromTop(6);
    ellipticalButton_.setBounds(buttonColumn.removeFromTop(kButtonHeight));

    if (presetBrowser_.isVisible()) {
        positionPresetBrowser();
    }
}

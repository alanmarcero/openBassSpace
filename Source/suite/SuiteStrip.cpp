#include "SuiteStrip.h"
#include <algorithm>

namespace alanmarcero::suite {
namespace {

const juce::Colour kBackground{ 0xff0d1317 };
const juce::Colour kPanel{ 0xff152026 };
const juce::Colour kEdge{ 0xff283842 };
const juce::Colour kMint{ 0xff48f0c8 };
const juce::Colour kGold{ 0xffffbe3b };
const juce::Colour kText{ 0xffe6f0f5 };
const juce::Colour kDim{ 0xffa3b6bf };

void paintPanel(juce::Graphics& g, juce::Rectangle<float> bounds) {
    g.setColour(kPanel);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(kEdge);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

} // namespace

PresetBrowser::PresetBrowser(PresetLibrary& library,
                             std::function<ParameterSnapshot()> capture,
                             std::function<void(const PresetEntry&)> load)
    : library_(library), capture_(std::move(capture)), load_(std::move(load)) {
    search_.setTextToShowWhenEmpty("Search presets", kDim);
    name_.setTextToShowWhenEmpty("Name for Save or Rename", kDim);
    for (auto* field : { &search_, &name_ }) {
        field->setColour(juce::TextEditor::backgroundColourId, kBackground);
        field->setColour(juce::TextEditor::textColourId, kText);
        field->setColour(juce::TextEditor::outlineColourId, kEdge);
        field->setColour(juce::TextEditor::focusedOutlineColourId, kMint);
        addAndMakeVisible(field);
    }
    search_.setTitle("Search presets");
    name_.setTitle("Preset name");
    list_.setTitle("Presets");
    search_.setComponentID("presetSearch");
    name_.setComponentID("presetName");
    list_.setComponentID("presetList");
    search_.setTooltip("Show only presets whose names contain this text.");
    name_.setTooltip("Type a name, then press Save for a new preset or Rename for the selected one.");
    save_.setTooltip("Save the current settings as a user preset with this name.");
    rename_.setTooltip("Give the selected user preset this name.");
    remove_.setTooltip("Delete the selected user preset.");
    favorite_.setTooltip("Mark or unmark the selected preset as a favorite.");
    search_.onTextChange = [this] { refresh(); };
    name_.onTextChange = [this] {
        disarm();
        updateButtons();
    };

    list_.setColour(juce::ListBox::backgroundColourId, kBackground);
    list_.setColour(juce::ListBox::outlineColourId, kEdge);
    list_.setRowHeight(28);
    addAndMakeVisible(list_);

    for (auto* button : { &save_, &rename_, &remove_, &favorite_ }) {
        button->setColour(juce::TextButton::buttonColourId, kPanel);
        button->setColour(juce::TextButton::buttonOnColourId, kMint);
        button->setColour(juce::TextButton::textColourOffId, kText);
        button->setColour(juce::TextButton::textColourOnId, kBackground);
        button->setMouseClickGrabsKeyboardFocus(false);
        button->setTitle(button->getButtonText());
        addAndMakeVisible(button);
    }

    save_.onClick = [this] {
        const auto name = name_.getText().trim();
        library_.rescan();
        if (armed_ != &save_ && library_.hasUser(name)) {
            arm(save_, "Replace?");
            return;
        }
        disarm();
        if (capture_ && library_.saveUser(name, capture_())) {
            refresh();
            if (onSaved) {
                onSaved(name);
            }
            if (onChanged) {
                onChanged();
            }
        }
    };
    rename_.onClick = [this] {
        disarm();
        if (const auto* entry = selected()) {
            if (!entry->factory && library_.renameUser(entry->name, name_.getText())) {
                refresh();
                if (onChanged) {
                    onChanged();
                }
            }
        }
    };
    remove_.onClick = [this] {
        const auto* entry = selected();
        if (entry == nullptr) {
            return;
        }
        if (armed_ != &remove_) {
            arm(remove_, "Delete?");
            return;
        }
        disarm();
        if (library_.deleteUser(entry->name)) {
            refresh();
            if (onChanged) {
                onChanged();
            }
        }
    };
    favorite_.onClick = [this] {
        disarm();
        if (const auto* entry = selected()) {
            library_.setFavorite(entry->name, entry->factory, !entry->favorite);
            refresh();
        }
    };

    setComponentID("presetBrowser");
    refresh();
}

// Deselects a preset the filter hides, so Rename and Delete never reach the row that takes its index.
void PresetBrowser::refresh() {
    const auto* current = selected();
    const auto selectedName = current != nullptr ? current->name : juce::String();
    const bool selectedFactory = current != nullptr && current->factory;
    shown_ = library_.filtered(search_.getText());
    list_.updateContent();
    const auto kept = std::find_if(shown_.begin(), shown_.end(), [&](const PresetEntry& entry) {
        return entry.name == selectedName && entry.factory == selectedFactory;
    });
    list_.deselectAllRows();
    if (kept != shown_.end()) {
        list_.selectRow(static_cast<int>(kept - shown_.begin()));
    }
    updateButtons();
    repaint();
}

void PresetBrowser::paint(juce::Graphics& g) {
    paintPanel(g, getLocalBounds().toFloat().reduced(0.5f));
}

void PresetBrowser::resized() {
    auto area = getLocalBounds().reduced(8);
    search_.setBounds(area.removeFromTop(28));
    area.removeFromTop(6);
    auto buttons = area.removeFromBottom(36);
    const int width = buttons.getWidth() / 4;
    save_.setBounds(buttons.removeFromLeft(width).reduced(2, 0));
    rename_.setBounds(buttons.removeFromLeft(width).reduced(2, 0));
    remove_.setBounds(buttons.removeFromLeft(width).reduced(2, 0));
    favorite_.setBounds(buttons.reduced(2, 0));
    area.removeFromBottom(6);
    name_.setBounds(area.removeFromBottom(28));
    area.removeFromBottom(6);
    list_.setBounds(area);
}

int PresetBrowser::getNumRows() {
    return static_cast<int>(shown_.size());
}

void PresetBrowser::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) {
    if (row < 0 || row >= static_cast<int>(shown_.size())) {
        return;
    }
    const auto& entry = shown_[static_cast<size_t>(row)];
    if (selected) {
        g.setColour(kMint.withAlpha(0.18f));
        g.fillRect(0, 0, width, height);
    }
    g.setColour(entry.favorite ? kGold : (entry.factory ? kDim : kText));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    const juce::String prefix = entry.favorite ? juce::String::charToString(0x2605) + " " : juce::String();
    const juce::String suffix = entry.factory ? "  factory" : "";
    g.drawText(prefix + entry.name + suffix, 8, 0, width - 12, height, juce::Justification::centredLeft, true);
}

void PresetBrowser::listBoxItemDoubleClicked(int row, const juce::MouseEvent&) {
    juce::ignoreUnused(row);
    loadSelected();
}

void PresetBrowser::selectedRowsChanged(int) {
    disarm();
    updateButtons();
}

void PresetBrowser::timerCallback() {
    disarm();
}

// The first click on Save over a user preset, or on Delete, shows a question for 3 s.
// A second click in that time commits.
void PresetBrowser::arm(juce::TextButton& button, const juce::String& question) {
    disarm();
    armed_ = &button;
    armedText_ = button.getButtonText();
    button.setButtonText(question);
    button.setTitle(question);
    startTimer(3000);
}

void PresetBrowser::disarm() {
    stopTimer();
    if (armed_ == nullptr) {
        return;
    }
    armed_->setButtonText(armedText_);
    armed_->setTitle(armedText_);
    armed_ = nullptr;
}

void PresetBrowser::updateButtons() {
    const auto* entry = selected();
    const bool user = entry != nullptr && !entry->factory;
    const auto name = name_.getText().trim();
    rename_.setEnabled(user && name.isNotEmpty());
    remove_.setEnabled(user);
    favorite_.setEnabled(entry != nullptr);
    favorite_.setButtonText(entry != nullptr && entry->favorite ? "Unfavorite" : "Favorite");
    favorite_.setTitle(favorite_.getButtonText());
    save_.setEnabled(name.isNotEmpty() && !library_.isFactoryName(name));
}

void PresetBrowser::loadSelected() {
    if (const auto* entry = selected()) {
        if (load_) {
            load_(*entry);
        }
        if (onChanged) {
            onChanged();
        }
    }
}

const PresetEntry* PresetBrowser::selected() const {
    const int row = list_.getSelectedRow();
    if (row < 0 || row >= static_cast<int>(shown_.size())) {
        return nullptr;
    }
    return &shown_[static_cast<size_t>(row)];
}

SuiteStrip::SuiteStrip(juce::AudioProcessorValueTreeState& state, const juce::String& bypassParameterId) {
    for (juce::TextButton* button : std::initializer_list<juce::TextButton*>{
             &undoButton_, &redoButton_, &aButton_, &bButton_, &copyAtoB_, &copyBtoA_,
             &bypassButton_, &presetButton_, &previousPreset_, &nextPreset_ }) {
        button->setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(button);
    }

    undoButton_.setComponentID("undoButton");
    redoButton_.setComponentID("redoButton");
    aButton_.setComponentID("compareA");
    bButton_.setComponentID("compareB");
    copyAtoB_.setComponentID("copyAtoB");
    copyBtoA_.setComponentID("copyBtoA");
    bypassButton_.setComponentID("bypassButton");
    presetButton_.setComponentID("presetButton");
    previousPreset_.setComponentID("previousPreset");
    nextPreset_.setComponentID("nextPreset");

    undoButton_.setTooltip("Undo the last parameter change.");
    redoButton_.setTooltip("Redo the last undone parameter change.");
    aButton_.setTooltip("Compare slot A.");
    bButton_.setTooltip("Compare slot B.");
    copyAtoB_.setTooltip("Copy slot A onto slot B.");
    copyBtoA_.setTooltip("Copy slot B onto slot A.");
    bypassButton_.setTooltip("Bypass the plug-in with a short crossfade.");
    presetButton_.setTooltip("Factory and user presets.");
    previousPreset_.setTooltip("Previous preset");
    nextPreset_.setTooltip("Next preset");
    previousPreset_.setTitle("Previous preset");
    nextPreset_.setTitle("Next preset");
    for (juce::TextButton* button : std::initializer_list<juce::TextButton*>{
             &undoButton_, &redoButton_, &copyAtoB_, &copyBtoA_, &bypassButton_ }) {
        button->setTitle(button->getButtonText());
    }
    aButton_.setTitle("Slot A");
    bButton_.setTitle("Slot B");

    undoButton_.onClick = [this] { if (actions_.undo) actions_.undo(); refresh(); };
    redoButton_.onClick = [this] { if (actions_.redo) actions_.redo(); refresh(); };
    aButton_.onClick = [this] { if (actions_.selectA) actions_.selectA(); refresh(); };
    bButton_.onClick = [this] { if (actions_.selectB) actions_.selectB(); refresh(); };
    copyAtoB_.onClick = [this] { if (actions_.copyAtoB) actions_.copyAtoB(); refresh(); };
    copyBtoA_.onClick = [this] { if (actions_.copyBtoA) actions_.copyBtoA(); refresh(); };
    presetButton_.onClick = [this] { if (actions_.togglePresetBrowser) actions_.togglePresetBrowser(); };
    previousPreset_.onClick = [this] { if (actions_.previousPreset) actions_.previousPreset(); refresh(); };
    nextPreset_.onClick = [this] { if (actions_.nextPreset) actions_.nextPreset(); refresh(); };

    bypassButton_.setClickingTogglesState(true);
    bypassButton_.onPopupMenu = [this] {
        if (actions_.onBypassPopupMenu) {
            actions_.onBypassPopupMenu(bypassButton_);
        }
    };
    bypassAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, bypassParameterId, bypassButton_);
    refresh();
}

void SuiteStrip::setActions(SuiteActions actions) {
    actions_ = std::move(actions);
    refresh();
}

void SuiteStrip::refresh() {
    const bool slotB = actions_.activeSlot && actions_.activeSlot() == 1;
    styleButton(aButton_, !slotB);
    styleButton(bButton_, slotB);
    styleButton(bypassButton_, bypassButton_.getToggleState());
    const bool bypassLocked = actions_.isBypassLocked && actions_.isBypassLocked();
    if (static_cast<bool>(bypassButton_.getProperties().getWithDefault("paramLocked", false)) != bypassLocked) {
        bypassButton_.getProperties().set("paramLocked", bypassLocked);
        bypassButton_.repaint();
    }
    for (auto* button : { &undoButton_, &redoButton_, &copyAtoB_, &copyBtoA_, &presetButton_, &previousPreset_, &nextPreset_ }) {
        styleButton(*button, false);
    }
    undoButton_.setEnabled(!actions_.canUndo || actions_.canUndo());
    redoButton_.setEnabled(!actions_.canRedo || actions_.canRedo());
    const auto name = actions_.presetName ? actions_.presetName() : juce::String();
    presetButton_.setButtonText(name.isNotEmpty() ? name : juce::String("Presets"));
    presetButton_.setTitle(presetButton_.getButtonText());
}

void SuiteStrip::paint(juce::Graphics& g) {
    paintPanel(g, getLocalBounds().toFloat().reduced(0.5f));
}

void SuiteStrip::resized() {
    auto row = getLocalBounds().reduced(4, 2);
    undoButton_.setBounds(row.removeFromLeft(56));
    row.removeFromLeft(4);
    redoButton_.setBounds(row.removeFromLeft(56));
    row.removeFromLeft(8);
    aButton_.setBounds(row.removeFromLeft(40));
    row.removeFromLeft(3);
    bButton_.setBounds(row.removeFromLeft(40));
    row.removeFromLeft(4);
    copyAtoB_.setBounds(row.removeFromLeft(92));
    row.removeFromLeft(3);
    copyBtoA_.setBounds(row.removeFromLeft(92));
    row.removeFromLeft(8);
    bypassButton_.setBounds(row.removeFromLeft(68));
    row.removeFromLeft(8);
    previousPreset_.setBounds(row.removeFromLeft(40));
    row.removeFromLeft(4);
    nextPreset_.setBounds(row.removeFromRight(40));
    row.removeFromRight(4);
    presetButton_.setBounds(row);
}

void SuiteStrip::styleButton(juce::TextButton& button, bool emphasised) const {
    button.setColour(juce::TextButton::buttonColourId, emphasised ? kMint : kBackground);
    button.setColour(juce::TextButton::buttonOnColourId, emphasised ? kGold : kMint);
    button.setColour(juce::TextButton::textColourOffId, emphasised ? kBackground : kText);
    button.setColour(juce::TextButton::textColourOnId, kBackground);
}

} // namespace alanmarcero::suite

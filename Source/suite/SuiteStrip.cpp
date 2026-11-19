#include "SuiteStrip.h"

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
    search_.setTextToShowWhenEmpty("Search or name a preset", kDim);
    search_.setColour(juce::TextEditor::backgroundColourId, kBackground);
    search_.setColour(juce::TextEditor::textColourId, kText);
    search_.setColour(juce::TextEditor::outlineColourId, kEdge);
    search_.setColour(juce::TextEditor::focusedOutlineColourId, kMint);
    search_.onTextChange = [this] { refresh(); };
    addAndMakeVisible(search_);

    list_.setColour(juce::ListBox::backgroundColourId, kBackground);
    list_.setColour(juce::ListBox::outlineColourId, kEdge);
    list_.setRowHeight(22);
    addAndMakeVisible(list_);

    for (auto* button : { &save_, &rename_, &remove_, &favorite_ }) {
        button->setColour(juce::TextButton::buttonColourId, kPanel);
        button->setColour(juce::TextButton::buttonOnColourId, kMint);
        button->setColour(juce::TextButton::textColourOffId, kText);
        button->setColour(juce::TextButton::textColourOnId, kBackground);
        addAndMakeVisible(button);
    }

    save_.onClick = [this] {
        if (capture_ && library_.saveUser(search_.getText(), capture_())) {
            refresh();
            if (onChanged) {
                onChanged();
            }
        }
    };
    rename_.onClick = [this] {
        if (const auto* entry = selected()) {
            if (!entry->factory && library_.renameUser(entry->name, search_.getText())) {
                refresh();
                if (onChanged) {
                    onChanged();
                }
            }
        }
    };
    remove_.onClick = [this] {
        if (const auto* entry = selected()) {
            if (library_.deleteUser(entry->name)) {
                refresh();
                if (onChanged) {
                    onChanged();
                }
            }
        }
    };
    favorite_.onClick = [this] {
        if (const auto* entry = selected()) {
            library_.setFavorite(entry->name, entry->factory, !entry->favorite);
            refresh();
        }
    };

    setComponentID("presetBrowser");
    refresh();
}

void PresetBrowser::refresh() {
    const auto selectedName = selected() != nullptr ? selected()->name : juce::String();
    const bool selectedFactory = selected() != nullptr && selected()->factory;
    shown_ = library_.filtered(search_.getText());
    list_.updateContent();
    for (int row = 0; row < static_cast<int>(shown_.size()); ++row) {
        if (shown_[static_cast<size_t>(row)].name == selectedName
            && shown_[static_cast<size_t>(row)].factory == selectedFactory) {
            list_.selectRow(row);
            break;
        }
    }
    updateButtons();
    repaint();
}

void PresetBrowser::paint(juce::Graphics& g) {
    paintPanel(g, getLocalBounds().toFloat().reduced(0.5f));
}

void PresetBrowser::resized() {
    auto area = getLocalBounds().reduced(8);
    search_.setBounds(area.removeFromTop(24));
    area.removeFromTop(6);
    auto buttons = area.removeFromBottom(26);
    const int width = buttons.getWidth() / 4;
    save_.setBounds(buttons.removeFromLeft(width).reduced(2, 0));
    rename_.setBounds(buttons.removeFromLeft(width).reduced(2, 0));
    remove_.setBounds(buttons.removeFromLeft(width).reduced(2, 0));
    favorite_.setBounds(buttons.reduced(2, 0));
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
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    const juce::String prefix = entry.favorite ? juce::String::charToString(0x2605) + " " : juce::String();
    const juce::String suffix = entry.factory ? "  factory" : "";
    g.drawText(prefix + entry.name + suffix, 8, 0, width - 12, height, juce::Justification::centredLeft, true);
}

void PresetBrowser::listBoxItemDoubleClicked(int row, const juce::MouseEvent&) {
    juce::ignoreUnused(row);
    loadSelected();
}

void PresetBrowser::selectedRowsChanged(int) {
    updateButtons();
}

void PresetBrowser::updateButtons() {
    const auto* entry = selected();
    const bool user = entry != nullptr && !entry->factory;
    rename_.setEnabled(user && search_.getText().trim().isNotEmpty());
    remove_.setEnabled(user);
    favorite_.setEnabled(entry != nullptr);
    favorite_.setButtonText(entry != nullptr && entry->favorite ? "Unfav" : "Fav");
    save_.setEnabled(search_.getText().trim().isNotEmpty() && !library_.isFactoryName(search_.getText()));
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
    for (auto* button : { &undoButton_, &redoButton_, &aButton_, &bButton_, &copyAtoB_, &copyBtoA_,
                          &bypassButton_, &presetButton_ }) {
        button->setWantsKeyboardFocus(false);
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

    undoButton_.setTooltip("Undo the last parameter change.");
    redoButton_.setTooltip("Redo the last undone parameter change.");
    aButton_.setTooltip("Compare slot A.");
    bButton_.setTooltip("Compare slot B.");
    copyAtoB_.setTooltip("Copy slot A onto slot B.");
    copyBtoA_.setTooltip("Copy slot B onto slot A.");
    bypassButton_.setTooltip("Bypass the plug-in with a short crossfade.");
    presetButton_.setTooltip("Factory and user presets.");

    undoButton_.onClick = [this] { if (actions_.undo) actions_.undo(); refresh(); };
    redoButton_.onClick = [this] { if (actions_.redo) actions_.redo(); refresh(); };
    aButton_.onClick = [this] { if (actions_.selectA) actions_.selectA(); refresh(); };
    bButton_.onClick = [this] { if (actions_.selectB) actions_.selectB(); refresh(); };
    copyAtoB_.onClick = [this] { if (actions_.copyAtoB) actions_.copyAtoB(); refresh(); };
    copyBtoA_.onClick = [this] { if (actions_.copyBtoA) actions_.copyBtoA(); refresh(); };
    presetButton_.onClick = [this] { if (actions_.togglePresetBrowser) actions_.togglePresetBrowser(); };

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
    for (auto* button : { &undoButton_, &redoButton_, &copyAtoB_, &copyBtoA_, &presetButton_ }) {
        styleButton(*button, false);
    }
    undoButton_.setEnabled(!actions_.canUndo || actions_.canUndo());
    redoButton_.setEnabled(!actions_.canRedo || actions_.canRedo());
    const auto name = actions_.presetName ? actions_.presetName() : juce::String();
    presetButton_.setButtonText(name.isNotEmpty() ? name : juce::String("Presets"));
}

void SuiteStrip::paint(juce::Graphics& g) {
    paintPanel(g, getLocalBounds().toFloat().reduced(0.5f));
}

void SuiteStrip::resized() {
    auto row = getLocalBounds().reduced(4, 3);
    undoButton_.setBounds(row.removeFromLeft(56));
    row.removeFromLeft(4);
    redoButton_.setBounds(row.removeFromLeft(56));
    row.removeFromLeft(8);
    aButton_.setBounds(row.removeFromLeft(30));
    row.removeFromLeft(3);
    bButton_.setBounds(row.removeFromLeft(30));
    row.removeFromLeft(4);
    copyAtoB_.setBounds(row.removeFromLeft(46));
    row.removeFromLeft(3);
    copyBtoA_.setBounds(row.removeFromLeft(46));
    row.removeFromLeft(8);
    bypassButton_.setBounds(row.removeFromLeft(70));
    row.removeFromLeft(8);
    presetButton_.setBounds(row);
}

void SuiteStrip::styleButton(juce::TextButton& button, bool emphasised) const {
    button.setColour(juce::TextButton::buttonColourId, emphasised ? kMint : kBackground);
    button.setColour(juce::TextButton::buttonOnColourId, emphasised ? kGold : kMint);
    button.setColour(juce::TextButton::textColourOffId, emphasised ? kBackground : kText);
    button.setColour(juce::TextButton::textColourOnId, kBackground);
}

} // namespace alanmarcero::suite

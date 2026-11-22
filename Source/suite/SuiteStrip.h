#pragma once

#include "PresetLibrary.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace alanmarcero::suite {

struct SuiteActions {
    std::function<void()> undo;
    std::function<void()> redo;
    std::function<bool()> canUndo;
    std::function<bool()> canRedo;
    std::function<void()> selectA;
    std::function<void()> selectB;
    std::function<void()> copyAtoB;
    std::function<void()> copyBtoA;
    std::function<int()> activeSlot;
    std::function<juce::String()> presetName;
    std::function<void()> togglePresetBrowser;
    std::function<void()> previousPreset;
    std::function<void()> nextPreset;
    std::function<bool()> isBypassLocked;
    std::function<void(juce::Component&)> onBypassPopupMenu;
};

class PopupTextButton final : public juce::TextButton {
public:
    using juce::TextButton::TextButton;
    std::function<void()> onPopupMenu;

    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) {
            if (onPopupMenu) {
                onPopupMenu();
            }
            return;
        }
        juce::TextButton::mouseDown(e);
    }
};

class PresetBrowser : public juce::Component, private juce::ListBoxModel, private juce::Timer {
public:
    PresetBrowser(PresetLibrary& library,
                  std::function<ParameterSnapshot()> capture,
                  std::function<void(const PresetEntry&)> load);

    void refresh();
    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onChanged;
    std::function<void(const juce::String&)> onSaved;

private:
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void timerCallback() override;

    void arm(juce::TextButton& button, const juce::String& question);
    void disarm();
    void updateButtons();
    void loadSelected();
    [[nodiscard]] const PresetEntry* selected() const;

    PresetLibrary& library_;
    std::function<ParameterSnapshot()> capture_;
    std::function<void(const PresetEntry&)> load_;
    std::vector<PresetEntry> shown_;
    juce::TextEditor search_;
    juce::TextEditor name_;
    juce::ListBox list_{"presets", this};
    juce::TextButton save_{"Save"};
    juce::TextButton rename_{"Rename"};
    juce::TextButton remove_{"Delete"};
    juce::TextButton favorite_{"Favorite"};
    juce::TextButton* armed_ = nullptr;
    juce::String armedText_;
};

// Compact undo, A/B, bypass, and preset controls. Colours match the openBassSpace faceplate.
class SuiteStrip : public juce::Component {
public:
    SuiteStrip(juce::AudioProcessorValueTreeState& state, const juce::String& bypassParameterId);

    void setActions(SuiteActions actions);
    void refresh();
    void paint(juce::Graphics& g) override;
    void resized() override;

    [[nodiscard]] juce::Rectangle<int> presetButtonBounds() const { return presetButton_.getBounds(); }

private:
    void styleButton(juce::TextButton& button, bool emphasised) const;

    SuiteActions actions_;
    juce::TextButton undoButton_{"Undo"};
    juce::TextButton redoButton_{"Redo"};
    juce::TextButton aButton_{"A"};
    juce::TextButton bButton_{"B"};
    juce::TextButton copyAtoB_{"Copy A to B"};
    juce::TextButton copyBtoA_{"Copy B to A"};
    PopupTextButton bypassButton_{"Bypass"};
    juce::TextButton presetButton_{"Presets"};
    juce::TextButton previousPreset_{"<"};
    juce::TextButton nextPreset_{">"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment_;
};

} // namespace alanmarcero::suite

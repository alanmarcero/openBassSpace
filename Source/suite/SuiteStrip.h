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
};

class PresetBrowser : public juce::Component, private juce::ListBoxModel {
public:
    PresetBrowser(PresetLibrary& library,
                  std::function<ParameterSnapshot()> capture,
                  std::function<void(const PresetEntry&)> load);

    void refresh();
    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onChanged;

private:
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void selectedRowsChanged(int lastRowSelected) override;

    void updateButtons();
    void loadSelected();
    [[nodiscard]] const PresetEntry* selected() const;

    PresetLibrary& library_;
    std::function<ParameterSnapshot()> capture_;
    std::function<void(const PresetEntry&)> load_;
    std::vector<PresetEntry> shown_;
    juce::TextEditor search_;
    juce::ListBox list_{"presets", this};
    juce::TextButton save_{"Save"};
    juce::TextButton rename_{"Rename"};
    juce::TextButton remove_{"Delete"};
    juce::TextButton favorite_{"Fav"};
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
    juce::TextButton copyAtoB_{"A>B"};
    juce::TextButton copyBtoA_{"B>A"};
    juce::TextButton bypassButton_{"Bypass"};
    juce::TextButton presetButton_{"Presets"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment_;
};

} // namespace alanmarcero::suite

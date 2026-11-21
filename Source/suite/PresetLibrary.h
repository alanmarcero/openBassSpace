#pragma once

#include "ParameterSnapshot.h"
#include <juce_core/juce_core.h>

namespace alanmarcero::suite {

struct PresetEntry {
    juce::String name;
    bool factory = false;
    bool favorite = false;
    ParameterSnapshot values;
};

// Factory presets are read-only. User presets live in
// ~/Documents/Alan Marcero/<pluginName>/Presets.
class PresetLibrary {
public:
    PresetLibrary(juce::String pluginName, std::vector<PresetEntry> factory);

    [[nodiscard]] const juce::File& directory() const noexcept { return directory_; }
    void setDirectory(juce::File directory);

    [[nodiscard]] static juce::File defaultDirectory(const juce::String& pluginName);

    void rescan();

    [[nodiscard]] const std::vector<PresetEntry>& factory() const noexcept { return factory_; }
    [[nodiscard]] const std::vector<PresetEntry>& user() const noexcept { return user_; }
    [[nodiscard]] std::vector<PresetEntry> filtered(const juce::String& query) const;

    // False when the name is empty or matches a factory preset.
    bool saveUser(const juce::String& name, const ParameterSnapshot& values);
    bool renameUser(const juce::String& from, const juce::String& to);
    bool deleteUser(const juce::String& name);
    bool setFavorite(const juce::String& name, bool factory, bool favorite);

    [[nodiscard]] bool isFactoryName(const juce::String& name) const;
    [[nodiscard]] bool hasUser(const juce::String& name) const;

private:
    [[nodiscard]] juce::File fileFor(const juce::String& name) const;
    [[nodiscard]] static juce::String safeFileName(juce::String name);
    PresetEntry* findUser(const juce::String& name);
    PresetEntry* findEntry(const juce::String& name, bool factory);
    void loadFavorites();
    void writeFavorites() const;

    juce::String pluginName_;
    juce::File directory_;
    std::vector<PresetEntry> factory_;
    std::vector<PresetEntry> user_;
};

} // namespace alanmarcero::suite

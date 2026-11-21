#include "PresetLibrary.h"

namespace alanmarcero::suite {
namespace {

constexpr const char* kPresetTag = "AlanMarceroPreset";

bool sameName(const juce::String& a, const juce::String& b) {
    return a.trim().equalsIgnoreCase(b.trim());
}

} // namespace

PresetLibrary::PresetLibrary(juce::String pluginName, std::vector<PresetEntry> factory)
    : pluginName_(std::move(pluginName)),
      directory_(defaultDirectory(pluginName_)),
      factory_(std::move(factory)) {
    for (auto& entry : factory_) {
        entry.factory = true;
    }
    rescan();
}

juce::File PresetLibrary::defaultDirectory(const juce::String& pluginName) {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Alan Marcero")
        .getChildFile(pluginName)
        .getChildFile("Presets");
}

void PresetLibrary::setDirectory(juce::File directory) {
    directory_ = std::move(directory);
    rescan();
}

juce::String PresetLibrary::safeFileName(juce::String name) {
    name = name.trim();
    juce::String cleaned;
    for (int i = 0; i < name.length(); ++i) {
        const juce::juce_wchar character = name[i];
        if (juce::CharacterFunctions::isLetterOrDigit(character)
            || character == ' ' || character == '-' || character == '_'
            || character == '(' || character == ')') {
            cleaned += character;
        }
    }
    cleaned = cleaned.trim().substring(0, 80);
    return cleaned.isEmpty() ? juce::String("Preset") : cleaned;
}

juce::File PresetLibrary::fileFor(const juce::String& name) const {
    return directory_.getChildFile(safeFileName(name) + ".preset");
}

bool PresetLibrary::isFactoryName(const juce::String& name) const {
    for (const auto& entry : factory_) {
        if (sameName(entry.name, name)) {
            return true;
        }
    }
    return false;
}

PresetEntry* PresetLibrary::findUser(const juce::String& name) {
    for (auto& entry : user_) {
        if (sameName(entry.name, name)) {
            return &entry;
        }
    }
    return nullptr;
}

PresetEntry* PresetLibrary::findEntry(const juce::String& name, bool factory) {
    auto& list = factory ? factory_ : user_;
    for (auto& entry : list) {
        if (sameName(entry.name, name)) {
            return &entry;
        }
    }
    return nullptr;
}

void PresetLibrary::loadFavorites() {
    for (auto& entry : factory_) {
        entry.favorite = false;
    }
    for (auto& entry : user_) {
        entry.favorite = false;
    }

    const auto file = directory_.getChildFile("favorites.txt");
    if (!file.existsAsFile()) {
        return;
    }

    juce::StringArray lines;
    lines.addLines(file.loadFileAsString());
    for (const auto& line : lines) {
        const auto tab = line.indexOfChar('\t');
        if (tab <= 0) {
            continue;
        }
        const auto kind = line.substring(0, tab).trim();
        const auto name = line.substring(tab + 1).trim();
        const bool factory = kind.equalsIgnoreCase("factory");
        if (auto* entry = findEntry(name, factory)) {
            entry->favorite = true;
        }
    }
}

void PresetLibrary::writeFavorites() const {
    juce::String text;
    const auto append = [&text](const std::vector<PresetEntry>& entries, const char* kind) {
        for (const auto& entry : entries) {
            if (entry.favorite) {
                text << kind << '\t' << entry.name << '\n';
            }
        }
    };
    append(factory_, "factory");
    append(user_, "user");

    directory_.createDirectory();
    directory_.getChildFile("favorites.txt").replaceWithText(text);
}

void PresetLibrary::rescan() {
    user_.clear();
    if (directory_.isDirectory()) {
        juce::Array<juce::File> files;
        directory_.findChildFiles(files, juce::File::findFiles, false, "*.preset");
        files.sort();
        for (const auto& file : files) {
            const auto xml = juce::parseXML(file);
            if (xml == nullptr || !xml->hasTagName(kPresetTag)) {
                continue;
            }
            PresetEntry entry;
            entry.factory = false;
            entry.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
            if (entry.name.isEmpty() || isFactoryName(entry.name)) {
                continue;
            }
            for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement()) {
                if (child->hasTagName("Value")) {
                    entry.values.emplace_back(child->getStringAttribute("id"),
                                              static_cast<float>(child->getDoubleAttribute("value")));
                }
            }
            user_.push_back(std::move(entry));
        }
    }
    loadFavorites();
}

std::vector<PresetEntry> PresetLibrary::filtered(const juce::String& query) const {
    std::vector<PresetEntry> all = factory_;
    all.insert(all.end(), user_.begin(), user_.end());
    const auto needle = query.trim();
    if (needle.isEmpty()) {
        return all;
    }

    std::vector<PresetEntry> matched;
    for (const auto& entry : all) {
        if (entry.name.containsIgnoreCase(needle)) {
            matched.push_back(entry);
        }
    }
    return matched;
}

bool PresetLibrary::saveUser(const juce::String& name, const ParameterSnapshot& values) {
    const auto trimmed = name.trim();
    if (trimmed.isEmpty() || isFactoryName(trimmed)) {
        return false;
    }

    directory_.createDirectory();
    juce::XmlElement xml(kPresetTag);
    xml.setAttribute("name", trimmed);
    for (const auto& [id, value] : values) {
        auto* child = xml.createNewChildElement("Value");
        child->setAttribute("id", id);
        child->setAttribute("value", static_cast<double>(value));
    }

    if (!xml.writeTo(fileFor(trimmed))) {
        return false;
    }
    rescan();
    return findUser(trimmed) != nullptr;
}

bool PresetLibrary::renameUser(const juce::String& from, const juce::String& to) {
    const auto source = from.trim();
    const auto target = to.trim();
    if (source.isEmpty() || target.isEmpty() || isFactoryName(source) || isFactoryName(target)) {
        return false;
    }

    auto* existing = findUser(source);
    if (existing == nullptr) {
        return false;
    }
    if (!sameName(source, target) && findUser(target) != nullptr) {
        return false;
    }

    const bool favorite = existing->favorite;
    const auto values = existing->values;
    const auto oldFile = fileFor(source);
    if (!saveUser(target, values)) {
        return false;
    }
    if (!sameName(source, target)) {
        oldFile.deleteFile();
    }
    rescan();
    if (favorite) {
        setFavorite(target, false, true);
    }
    return findUser(target) != nullptr && (sameName(source, target) || findUser(source) == nullptr);
}

bool PresetLibrary::deleteUser(const juce::String& name) {
    const auto trimmed = name.trim();
    if (trimmed.isEmpty() || isFactoryName(trimmed) || findUser(trimmed) == nullptr) {
        return false;
    }

    fileFor(trimmed).deleteFile();
    rescan();
    return findUser(trimmed) == nullptr;
}

// Rescans first, so the favorites file keeps changes made by other plug-in instances.
bool PresetLibrary::setFavorite(const juce::String& name, bool factory, bool favorite) {
    rescan();
    auto* entry = findEntry(name, factory);
    if (entry == nullptr) {
        return false;
    }
    entry->favorite = favorite;
    writeFavorites();
    return true;
}

} // namespace alanmarcero::suite

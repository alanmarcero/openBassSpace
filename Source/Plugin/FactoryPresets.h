#pragma once

#include "ParameterSnapshot.h"
#include "Parameters.h"
#include "PresetLibrary.h"
#include <array>

namespace bass_space {

struct FactoryPreset {
    const char* name;
    float frequency;
    float width;
    float upperWidth;
    float stereoize;
    float outputDb;
    float subDrive;
    float upperTilt;
    float soloLow;
};

// Program 0 matches the parameter defaults (mono bass under 120 Hz).
inline const std::array<FactoryPreset, 9>& factoryPresets() {
    static const std::array<FactoryPreset, 9> presets {{
        { "Mono under 120 Hz", 120.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { "Mono under 80 Hz", 80.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { "Mono under 200 Hz", 200.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { "Flat", 120.0f, 100.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { "Wide upper only", 120.0f, 0.0f, 180.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { "Stereoize", 150.0f, 0.0f, 110.0f, 45.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { "Solo bass", 120.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f },
        { "Sub punch", 90.0f, 0.0f, 100.0f, 0.0f, 0.0f, 55.0f, 0.0f, 0.0f },
        { "Air tilt", 140.0f, 15.0f, 140.0f, 20.0f, 0.0f, 0.0f, 3.5f, 0.0f },
    }};
    return presets;
}

inline alanmarcero::suite::ParameterSnapshot valuesFor(const FactoryPreset& preset) {
    using namespace params;
    return {
        { kFrequency, preset.frequency },
        { kWidth, preset.width },
        { kUpperWidth, preset.upperWidth },
        { kStereoize, preset.stereoize },
        { kOutputDb, preset.outputDb },
        { kSubDrive, preset.subDrive },
        { kUpperTilt, preset.upperTilt },
        { kSoloLow, preset.soloLow },
        { kBypass, 0.0f },
    };
}

inline std::vector<alanmarcero::suite::PresetEntry> factoryPresetEntries() {
    std::vector<alanmarcero::suite::PresetEntry> entries;
    entries.reserve(factoryPresets().size());
    for (const auto& preset : factoryPresets()) {
        alanmarcero::suite::PresetEntry entry;
        entry.name = preset.name;
        entry.factory = true;
        entry.values = valuesFor(preset);
        entries.push_back(std::move(entry));
    }
    return entries;
}

} // namespace bass_space

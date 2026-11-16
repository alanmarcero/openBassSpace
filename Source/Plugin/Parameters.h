#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace bass_space::params {

inline constexpr const char* kFrequency = "frequency";
inline constexpr const char* kWidth = "width";
inline constexpr const char* kUpperWidth = "upper_width";
inline constexpr const char* kStereoize = "stereoize";
inline constexpr const char* kOutputDb = "output_db";

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace bass_space::params

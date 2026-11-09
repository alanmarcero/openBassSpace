#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace bass_space::params {

inline constexpr const char* kFrequency = "frequency";
inline constexpr const char* kWidth = "width";

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace bass_space::params

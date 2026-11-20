#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace bass_space::params {

inline constexpr const char* kFrequency = "frequency";
inline constexpr const char* kWidth = "width";
inline constexpr const char* kUpperWidth = "upper_width";
inline constexpr const char* kStereoize = "stereoize";
inline constexpr const char* kOutputDb = "output_db";
inline constexpr const char* kSubDrive = "sub_drive";
inline constexpr const char* kUpperTilt = "upper_tilt";
inline constexpr const char* kSoloLow = "solo_low";
inline constexpr const char* kBypass = "bypass";
inline constexpr const char* kSlope = "slope";
inline constexpr const char* kElliptical = "elliptical";

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace bass_space::params

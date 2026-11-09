#include "Parameters.h"

namespace bass_space::params {

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    // Frequency: 20 Hz to 500 Hz, default 120 Hz, skewed to emphasize 60-200 Hz
    auto freqRange = juce::NormalisableRange<float>(20.0f, 500.0f, 0.5f, 0.45f);
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kFrequency, 1 },
        "Frequency",
        freqRange,
        120.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")
    ));

    // Width: 0% to 200%, default 0% (Mono), 100% is neutral, 200% is wide
    auto widthRange = juce::NormalisableRange<float>(0.0f, 200.0f, 0.5f, 1.0f);
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ kWidth, 1 },
        "Width",
        widthRange,
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")
    ));

    return { parameters.begin(), parameters.end() };
}

} // namespace bass_space::params

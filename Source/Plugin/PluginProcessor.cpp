#include "PluginProcessor.h"
#include "PluginEditor.h"

BassSpaceProcessor::BassSpaceProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("OpenBassSpaceParameters"),
                 bass_space::params::createParameterLayout()) {
    freqParam_ = parameters.getRawParameterValue(bass_space::params::kFrequency);
    widthParam_ = parameters.getRawParameterValue(bass_space::params::kWidth);
}

void BassSpaceProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    shaper_.prepare(sampleRate);
    reset();
}

void BassSpaceProcessor::releaseResources() {
    shaper_.reset();
}

void BassSpaceProcessor::reset() {
    shaper_.reset();
}

bool BassSpaceProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& mainInput = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput != juce::AudioChannelSet::stereo() && mainInput != juce::AudioChannelSet::mono()) {
        return false;
    }

    return mainInput == mainOutput;
}

void BassSpaceProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/) {
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, numSamples);
    }

    if (numChannels == 0 || numSamples == 0 || totalNumInputChannels == 0) {
        return;
    }

    const float freq = currentFrequency();
    const float widthRatio = currentWidth() * 0.01f; // 0% = 0.0 (mono), 100% = 1.0 (flat), 200% = 2.0 (wide)

    shaper_.setCrossoverFrequency(freq);
    shaper_.setWidth(widthRatio);

    float* left = buffer.getWritePointer(0);
    float* right = (numChannels > 1 && totalNumInputChannels > 1) ? buffer.getWritePointer(1) : nullptr;
    shaper_.processBlock(left, right, numSamples);
}

juce::AudioProcessorEditor* BassSpaceProcessor::createEditor() {
    return new BassSpaceEditor(*this);
}

void BassSpaceProcessor::getStateInformation(juce::MemoryBlock& destination) {
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destination);
}

void BassSpaceProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr || !xmlState->hasTagName(parameters.state.getType())) {
        return;
    }

    parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new BassSpaceProcessor();
}

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
    upperWidthParam_ = parameters.getRawParameterValue(bass_space::params::kUpperWidth);
    stereoizeParam_ = parameters.getRawParameterValue(bass_space::params::kStereoize);
    outputDbParam_ = parameters.getRawParameterValue(bass_space::params::kOutputDb);
}

void BassSpaceProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    processingRate_ = sampleRate;
    shaper_.prepare(sampleRate);
    outputGain_.reset(sampleRate, 0.02);
    reset();
}

void BassSpaceProcessor::releaseResources() {
    shaper_.reset();
}

void BassSpaceProcessor::reset() {
    artworkClock_.reset();
    shaper_.reset();
    const float decibels = outputDbParam_ != nullptr ? outputDbParam_->load(std::memory_order_relaxed) : 0.0f;
    outputGain_.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(decibels));
    correlation_.store(0.0f, std::memory_order_relaxed);
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
    auto* playHead = getPlayHead();
    artworkClock_.update(playHead ? playHead->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo>{},
                         numSamples, processingRate_);
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
    shaper_.setUpperWidth(currentUpperWidth() * 0.01f);
    shaper_.setStereoizeAmount(currentStereoize() * 0.01f);

    float* left = buffer.getWritePointer(0);
    float* right = (numChannels > 1 && totalNumInputChannels > 1) ? buffer.getWritePointer(1) : nullptr;
    shaper_.processBlock(left, right, numSamples);

    const float decibels = outputDbParam_ != nullptr ? outputDbParam_->load(std::memory_order_relaxed) : 0.0f;
    outputGain_.setTargetValue(juce::Decibels::decibelsToGain(decibels));
    float* channels[2] = { buffer.getWritePointer(0),
                           numChannels > 1 ? buffer.getWritePointer(1) : nullptr };
    double cross = 0.0;
    double leftEnergy = 0.0;
    double rightEnergy = 0.0;
    for (int sample = 0; sample < numSamples; ++sample) {
        const float gain = outputGain_.getNextValue();
        for (int channel = 0; channel < numChannels; ++channel) {
            channels[channel][sample] *= gain;
        }
        const double leftValue = channels[0][sample];
        const double rightValue = channels[1] != nullptr ? channels[1][sample] : leftValue;
        cross += leftValue * rightValue;
        leftEnergy += leftValue * leftValue;
        rightEnergy += rightValue * rightValue;
    }
    correlation_.store(static_cast<float>(analog_master::core::StereoFieldShaper::correlationCoefficient(
                            cross, leftEnergy, rightEnergy)),
                        std::memory_order_relaxed);
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

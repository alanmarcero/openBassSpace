#include "PluginProcessor.h"
#include "FactoryPresets.h"
#include "PluginEditor.h"
#include <cmath>

namespace {

juce::ValueTree slotTree(int index, const alanmarcero::suite::ParameterSnapshot& values) {
    auto tree = alanmarcero::suite::snapshotToTree("Slot", values);
    tree.setProperty("index", index, nullptr);
    return tree;
}

alanmarcero::suite::ParameterSnapshot snapshotFromSlot(const juce::ValueTree& tree) {
    if (tree.getNumChildren() > 0 && tree.getChild(0).hasType("P")) {
        return alanmarcero::suite::snapshotFromTree(tree);
    }
    return {};
}

} // namespace

BassSpaceProcessor::BassSpaceProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("OpenBassSpaceParameters"),
                 bass_space::params::createParameterLayout()),
      presets_("openBassSpace", bass_space::factoryPresetEntries()),
      history_(*this,
               [this] { return captureMemory(); },
               [this](const juce::ValueTree& memory) { restoreMemory(memory); }) {
    freqParam_ = parameters.getRawParameterValue(bass_space::params::kFrequency);
    widthParam_ = parameters.getRawParameterValue(bass_space::params::kWidth);
    upperWidthParam_ = parameters.getRawParameterValue(bass_space::params::kUpperWidth);
    stereoizeParam_ = parameters.getRawParameterValue(bass_space::params::kStereoize);
    outputDbParam_ = parameters.getRawParameterValue(bass_space::params::kOutputDb);
    subDriveParam_ = parameters.getRawParameterValue(bass_space::params::kSubDrive);
    upperTiltParam_ = parameters.getRawParameterValue(bass_space::params::kUpperTilt);
    soloLowParam_ = parameters.getRawParameterValue(bass_space::params::kSoloLow);
    bypassParam_ = parameters.getRawParameterValue(bass_space::params::kBypass);

    currentProgram_ = 0;
    currentPresetName_ = bass_space::factoryPresets().front().name;
    slots_[0] = currentParameterValues();
    slots_[1] = slots_[0];
}

void BassSpaceProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    processingRate_ = sampleRate;
    shaper_.prepare(sampleRate);
    outputGain_.reset(sampleRate, 0.02);
    bypassMix_.reset(sampleRate, 0.010);
    dryBuffer_.setSize(2, juce::jmax(samplesPerBlock, 4096), false, false, true);
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
    bypassMix_.setCurrentAndTargetValue(currentBypass() ? 0.0f : 1.0f);
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

void BassSpaceProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    processAudio(buffer, midi, false);
}

void BassSpaceProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    processAudio(buffer, midi, true);
}

void BassSpaceProcessor::processAudio(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/, bool forceBypass) {
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

    if (dryBuffer_.getNumChannels() < numChannels || dryBuffer_.getNumSamples() < numSamples) {
        dryBuffer_.setSize(juce::jmax(numChannels, dryBuffer_.getNumChannels()),
                           juce::jmax(numSamples, dryBuffer_.getNumSamples()),
                           false, false, true);
    }
    for (int channel = 0; channel < numChannels; ++channel) {
        dryBuffer_.copyFrom(channel, 0, buffer, channel, 0, numSamples);
    }

    const float freq = currentFrequency();
    const float widthRatio = currentWidth() * 0.01f;

    shaper_.setCrossoverFrequency(freq);
    shaper_.setWidth(widthRatio);
    shaper_.setUpperWidth(currentUpperWidth() * 0.01f);
    shaper_.setStereoizeAmount(currentStereoize() * 0.01f);
    shaper_.setSubDrive(currentSubDrive() * 0.01f);
    shaper_.setUpperTiltDb(currentUpperTilt());
    shaper_.setSoloLow(currentSoloLow());

    float* left = buffer.getWritePointer(0);
    float* right = (numChannels > 1 && totalNumInputChannels > 1) ? buffer.getWritePointer(1) : nullptr;
    shaper_.processBlock(left, right, numSamples);

    const float decibels = outputDbParam_ != nullptr ? outputDbParam_->load(std::memory_order_relaxed) : 0.0f;
    outputGain_.setTargetValue(juce::Decibels::decibelsToGain(decibels));
    const bool bypassTarget = forceBypass || currentBypass();
    bypassMix_.setTargetValue(bypassTarget ? 0.0f : 1.0f);
    const bool mixBypass = bypassTarget || bypassMix_.isSmoothing();

    float* channels[2] = { buffer.getWritePointer(0),
                           numChannels > 1 ? buffer.getWritePointer(1) : nullptr };
    double cross = 0.0;
    double leftEnergy = 0.0;
    double rightEnergy = 0.0;
    for (int sample = 0; sample < numSamples; ++sample) {
        const float gain = outputGain_.getNextValue();
        const float mix = mixBypass ? bypassMix_.getNextValue() : 1.0f;
        for (int channel = 0; channel < numChannels; ++channel) {
            const float dry = dryBuffer_.getSample(channel, sample);
            const float wet = channels[channel][sample] * gain;
            channels[channel][sample] = dry + mix * (wet - dry);
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

juce::AudioProcessorParameter* BassSpaceProcessor::getBypassParameter() const {
    return parameters.getParameter(bass_space::params::kBypass);
}

int BassSpaceProcessor::getNumPrograms() {
    return static_cast<int>(bass_space::factoryPresets().size());
}

int BassSpaceProcessor::getCurrentProgram() {
    return juce::jlimit(0, getNumPrograms() - 1, currentProgram_);
}

void BassSpaceProcessor::setCurrentProgram(int index) {
    const auto& bank = bass_space::factoryPresets();
    if (index < 0 || index >= static_cast<int>(bank.size())) {
        return;
    }

    alanmarcero::suite::PresetEntry entry;
    entry.name = bank[static_cast<size_t>(index)].name;
    entry.factory = true;
    entry.values = bass_space::valuesFor(bank[static_cast<size_t>(index)]);
    loadPreset(entry);
}

const juce::String BassSpaceProcessor::getProgramName(int index) {
    const auto& bank = bass_space::factoryPresets();
    if (index < 0 || index >= static_cast<int>(bank.size())) {
        return {};
    }
    return bank[static_cast<size_t>(index)].name;
}

void BassSpaceProcessor::setEditorSize(int width, int height) noexcept {
    editorWidth_ = juce::jlimit(minEditorWidth, 1800, width);
    editorHeight_ = juce::jlimit(minEditorHeight, 1400, height);
}

void BassSpaceProcessor::undo() {
    history_.undo.undo();
}

void BassSpaceProcessor::redo() {
    history_.undo.redo();
}

bool BassSpaceProcessor::canUndo() const {
    return history_.undo.canUndo();
}

bool BassSpaceProcessor::canRedo() const {
    return history_.undo.canRedo();
}

void BassSpaceProcessor::selectCompareSlot(int slot) {
    if ((slot != 0 && slot != 1) || slot == activeSlot_) {
        return;
    }

    history_.performMutation("A/B", [this] {
        activeSlot_ ^= 1;
        applyValues(slots_[static_cast<size_t>(activeSlot_)]);
    });
}

void BassSpaceProcessor::copyCompareSlot(int from, int to) {
    if ((from != 0 && from != 1) || (to != 0 && to != 1) || from == to) {
        return;
    }

    history_.performMutation(from == 0 ? "Copy A to B" : "Copy B to A", [this, from, to] {
        if (from == activeSlot_) {
            slots_[static_cast<size_t>(from)] = currentParameterValues();
        }
        slots_[static_cast<size_t>(to)] = slots_[static_cast<size_t>(from)];
        if (to == activeSlot_) {
            applyValues(slots_[static_cast<size_t>(to)]);
        }
    });
}

void BassSpaceProcessor::loadPreset(const alanmarcero::suite::PresetEntry& preset) {
    history_.performMutation("Preset", [this, preset] {
        applyValues(preset.values);
        currentPresetName_ = preset.name;
        if (!preset.factory) {
            return;
        }
        const auto& bank = bass_space::factoryPresets();
        for (int i = 0; i < static_cast<int>(bank.size()); ++i) {
            if (preset.name.equalsIgnoreCase(bank[static_cast<size_t>(i)].name)) {
                currentProgram_ = i;
                return;
            }
        }
    });
}

void BassSpaceProcessor::setUserPresetDirectory(const juce::File& directory) {
    presets_.setDirectory(directory);
}

alanmarcero::suite::ParameterSnapshot BassSpaceProcessor::currentParameterValues() const {
    alanmarcero::suite::ParameterSnapshot values;
    for (auto* parameter : getParameters()) {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter)) {
            values.emplace_back(ranged->paramID, ranged->convertFrom0to1(ranged->getValue()));
        }
    }
    return values;
}

void BassSpaceProcessor::applyValues(const alanmarcero::suite::ParameterSnapshot& values) {
    for (const auto& [id, value] : values) {
        if (auto* parameter = parameters.getParameter(id)) {
            const float normalised = parameter->convertTo0to1(value);
            if (std::abs(parameter->getValue() - normalised) > 1.0e-5f) {
                parameter->setValueNotifyingHost(normalised);
            }
        }
    }
}

juce::ValueTree BassSpaceProcessor::captureMemory() {
    auto values = currentParameterValues();
    slots_[static_cast<size_t>(activeSlot_)] = values;

    juce::ValueTree memory("SuiteMemory");
    memory.setProperty("activeSlot", activeSlot_, nullptr);
    memory.setProperty("program", currentProgram_, nullptr);
    memory.setProperty("presetName", currentPresetName_, nullptr);
    memory.appendChild(alanmarcero::suite::snapshotToTree("Params", values), nullptr);
    memory.appendChild(alanmarcero::suite::snapshotToTree("Slot0", slots_[0]), nullptr);
    memory.appendChild(alanmarcero::suite::snapshotToTree("Slot1", slots_[1]), nullptr);
    return memory;
}

void BassSpaceProcessor::restoreMemory(const juce::ValueTree& memory) {
    activeSlot_ = juce::jlimit(0, 1, static_cast<int>(memory.getProperty("activeSlot", activeSlot_)));
    currentProgram_ = juce::jlimit(0, getNumPrograms() - 1, static_cast<int>(memory.getProperty("program", currentProgram_)));
    currentPresetName_ = memory.getProperty("presetName", currentPresetName_).toString();
    if (auto params = memory.getChildWithName("Params"); params.isValid()) {
        applyValues(alanmarcero::suite::snapshotFromTree(params));
    }
    if (auto slot = memory.getChildWithName("Slot0"); slot.isValid()) {
        slots_[0] = alanmarcero::suite::snapshotFromTree(slot);
    }
    if (auto slot = memory.getChildWithName("Slot1"); slot.isValid()) {
        slots_[1] = alanmarcero::suite::snapshotFromTree(slot);
    }
}

void BassSpaceProcessor::getStateInformation(juce::MemoryBlock& destination) {
    slots_[static_cast<size_t>(activeSlot_)] = currentParameterValues();

    juce::XmlElement root("OpenBassSpaceState");
    root.setAttribute("editorWidth", editorWidth_);
    root.setAttribute("editorHeight", editorHeight_);
    root.setAttribute("activeSlot", activeSlot_);
    root.setAttribute("currentProgram", currentProgram_);
    root.setAttribute("presetName", currentPresetName_);
    if (auto parameterXml = parameters.copyState().createXml()) {
        root.addChildElement(parameterXml.release());
    }
    for (int slot = 0; slot < 2; ++slot) {
        auto xml = slotTree(slot, slots_[static_cast<size_t>(slot)]).createXml();
        root.addChildElement(xml.release());
    }
    copyXmlToBinary(root, destination);
}

void BassSpaceProcessor::restoreParameterTree(juce::ValueTree tree) {
    if (!tree.isValid()) {
        return;
    }

    const auto ensureDefault = [&tree](const char* paramId, float defaultValue) {
        if (!tree.getChildWithProperty("id", paramId).isValid()) {
            juce::ValueTree child("PARAM");
            child.setProperty("id", paramId, nullptr);
            child.setProperty("value", defaultValue, nullptr);
            tree.appendChild(child, nullptr);
        }
    };
    ensureDefault(bass_space::params::kUpperWidth, 100.0f);
    ensureDefault(bass_space::params::kStereoize, 0.0f);
    ensureDefault(bass_space::params::kSubDrive, 0.0f);
    ensureDefault(bass_space::params::kUpperTilt, 0.0f);
    ensureDefault(bass_space::params::kSoloLow, 0.0f);
    ensureDefault(bass_space::params::kBypass, 0.0f);
    parameters.replaceState(tree);
}

void BassSpaceProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr) {
        return;
    }

    if (xmlState->hasTagName(parameters.state.getType())) {
        restoreParameterTree(juce::ValueTree::fromXml(*xmlState));
        slots_[0] = currentParameterValues();
        slots_[1] = slots_[0];
        activeSlot_ = 0;
        currentProgram_ = 0;
        currentPresetName_ = getProgramName(0);
        history_.clear();
        return;
    }

    if (!xmlState->hasTagName("OpenBassSpaceState")) {
        return;
    }

    if (auto* parameterXml = xmlState->getChildByName(parameters.state.getType().toString())) {
        restoreParameterTree(juce::ValueTree::fromXml(*parameterXml));
    }

    editorWidth_ = juce::jlimit(minEditorWidth, 1800, xmlState->getIntAttribute("editorWidth", editorWidth_));
    editorHeight_ = juce::jlimit(minEditorHeight, 1400, xmlState->getIntAttribute("editorHeight", editorHeight_));
    activeSlot_ = juce::jlimit(0, 1, xmlState->getIntAttribute("activeSlot", 0));
    currentProgram_ = juce::jlimit(0, getNumPrograms() - 1, xmlState->getIntAttribute("currentProgram", 0));
    currentPresetName_ = xmlState->getStringAttribute("presetName", getProgramName(currentProgram_));

    slots_[0] = currentParameterValues();
    slots_[1] = slots_[0];
    for (auto* child = xmlState->getFirstChildElement(); child != nullptr; child = child->getNextElement()) {
        if (!child->hasTagName("Slot")) {
            continue;
        }
        const int index = juce::jlimit(0, 1, child->getIntAttribute("index"));
        auto values = snapshotFromSlot(juce::ValueTree::fromXml(*child));
        if (!values.empty()) {
            slots_[static_cast<size_t>(index)] = std::move(values);
        }
    }

    history_.clear();
    if (auto* editor = getActiveEditor()) {
        editor->setSize(editorWidth_, editorHeight_);
    }
}

// The VST3, AU, and standalone wrappers call this. It has to live in a file the plugin target compiles.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new BassSpaceProcessor();
}

#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

static void require(bool b, const char* message) {
    if (!b) {
        throw std::runtime_error(message);
    }
}

static void pump() {
    juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
}

static void save(juce::Component& c, const juce::String& path) {
    juce::File f(path);
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::FileOutputStream out(f);
    require(out.openedOk(), "screenshot output could not be opened");
    require(juce::PNGImageFormat().writeImageToStream(
                c.createComponentSnapshot(c.getLocalBounds(), true, 2.0f), out),
            "capture failed");
}

static bool hasVisibleLabel(juce::Component& component, const juce::String& text) {
    if (auto* label = dynamic_cast<juce::Label*>(&component);
        label != nullptr && label->getText() == text && !label->getBounds().isEmpty()) {
        return true;
    }
    return std::any_of(component.getChildren().begin(), component.getChildren().end(), [&text](auto* child) {
        return child->isVisible() && hasVisibleLabel(*child, text);
    });
}

static juce::Label* findLabel(juce::Component& component, const juce::String& text) {
    if (auto* label = dynamic_cast<juce::Label*>(&component); label != nullptr && label->getText() == text) {
        return label;
    }
    for (auto* child : component.getChildren()) {
        if (auto* found = findLabel(*child, text)) {
            return found;
        }
    }
    return nullptr;
}

static juce::Component* findById(juce::Component& component, const juce::String& id) {
    if (component.getComponentID() == id) {
        return &component;
    }
    for (auto* child : component.getChildren()) {
        if (auto* found = findById(*child, id)) {
            return found;
        }
    }
    return nullptr;
}

static juce::Slider* findSlider(juce::Component& component, const juce::String& title) {
    const auto& children = component.getChildren();
    const auto found = std::find_if(children.begin(), children.end(), [&title](auto* child) {
        auto* slider = dynamic_cast<juce::Slider*>(child);
        return slider != nullptr && slider->getTitle() == title;
    });
    if (found == children.end()) {
        return nullptr;
    }
    return dynamic_cast<juce::Slider*>(*found);
}

static juce::Label* valueBoxOf(juce::Slider& slider) {
    const auto& parts = slider.getChildren();
    const auto box = std::find_if(parts.begin(), parts.end(), [](auto* part) {
        return dynamic_cast<juce::Label*>(part) != nullptr;
    });
    require(box != parts.end(), "slider shows its value box");
    return dynamic_cast<juce::Label*>(*box);
}

// Names the first two visible children whose bounds overlap, or returns an empty string.
static juce::String overlappingChildren(juce::Component& parent) {
    const auto describe = [](juce::Component* component) {
        if (auto* label = dynamic_cast<juce::Label*>(component)) {
            return label->getText();
        }
        return component->getTitle() + component->getComponentID();
    };
    const auto& children = parent.getChildren();
    for (int first = 0; first < children.size(); ++first) {
        for (int second = first + 1; second < children.size(); ++second) {
            auto* a = children[first];
            auto* b = children[second];
            if (a->isVisible() && b->isVisible() && a->getBounds().intersects(b->getBounds())) {
                return describe(a) + " overlaps " + describe(b);
            }
        }
    }
    return {};
}

static int smallestDial(juce::Component& parent) {
    int smallest = std::numeric_limits<int>::max();
    for (auto* child : parent.getChildren()) {
        if (auto* slider = dynamic_cast<juce::Slider*>(child); slider != nullptr && slider->isVisible()) {
            smallest = juce::jmin(smallest, slider->getWidth(), slider->getHeight() - slider->getTextBoxHeight());
        }
    }
    return smallest;
}

static int changedPixels(const juce::Image& before, const juce::Image& after) {
    int changed = 0;
    for (int y = 0; y < before.getHeight(); ++y) {
        for (int x = 0; x < before.getWidth(); ++x) {
            changed += before.getPixelAt(x, y) != after.getPixelAt(x, y) ? 1 : 0;
        }
    }
    return changed;
}

static void checkLayout(juce::Component& editor, const juce::String& when) {
    const auto overlap = overlappingChildren(editor);
    require(overlap.isEmpty(), (when + ": " + overlap).toRawUTF8());
    require(smallestDial(editor) >= 64, (when + ": every dial is at least 64 px").toRawUTF8());
    const auto clearance = [&editor](const juce::String& upperTitle, const juce::String& lowerName) {
        auto* upper = findSlider(editor, upperTitle);
        auto* lower = findLabel(editor, lowerName);
        require(upper != nullptr && lower != nullptr, "stacked knobs exist");
        return lower->getY() - editor.getLocalArea(upper, valueBoxOf(*upper)->getBounds()).getBottom();
    };
    require(clearance("Crossover frequency", "Stereoize") >= 12 && clearance("Bass stereo width", "Upper width") >= 12,
            (when + ": each stacked value sits clear of the next knob's name").toRawUTF8());
}

// Factory-default editor at the default window size. The host-check path below
// moves parameters before it saves, so the published preview uses this mode.
static int captureFactoryPreview(const juce::String& outPath) {
    BassSpaceProcessor processor;
    processor.setRateAndBufferSizeDetails(48000, 256);
    processor.prepareToPlay(48000, 256);
    require(processor.getCurrentProgram() == 0, "factory default program");
    require(std::abs(processor.currentFrequency() - 120.0f) < 0.001f, "factory crossover");
    require(std::abs(processor.currentWidth()) < 0.001f, "factory bass width");
    require(std::abs(processor.currentUpperWidth() - 100.0f) < 0.001f, "factory upper width");
    require(std::abs(processor.currentStereoize()) < 0.001f, "factory stereoize");
    require(std::abs(processor.parameters.getRawParameterValue("output_db")->load()) < 0.001f,
            "factory output");
    require(std::abs(processor.currentSubDrive()) < 0.001f, "factory sub drive");
    require(std::abs(processor.currentUpperTilt()) < 0.001f, "factory upper tilt");
    require(!processor.currentSoloLow(), "factory solo bass off");
    require(!processor.currentBypass(), "factory bypass off");
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    require(editor != nullptr, "Editor creation failed");
    require(editor->getWidth() == BassSpaceProcessor::defaultEditorWidth
                && editor->getHeight() == BassSpaceProcessor::defaultEditorHeight,
            "editor size match");
    pump();
    save(*editor, outPath);
    std::cout << "factory preview saved to " << outPath << " ("
              << editor->getWidth() << "x" << editor->getHeight() << ")\n";
    return 0;
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 1 && juce::String(argv[1]) == "--preview") {
        const juce::String outPath = argc > 2 ? juce::String(argv[2]) : "docs/screenshots/preview.png";
        return captureFactoryPreview(outPath);
    }
    BassSpaceProcessor p;
    p.setRateAndBufferSizeDetails(48000, 256);
    p.prepareToPlay(48000, 256);

    for (auto* parameter : p.getParameters()) {
        auto* floatParameter = dynamic_cast<juce::AudioParameterFloat*>(parameter);
        if (floatParameter == nullptr) {
            continue;
        }
        for (const float normalised : { 0.0f, 0.13f, 0.5f, 0.87f, 1.0f, parameter->getDefaultValue() }) {
            const auto text = parameter->getText(normalised, 0);
            const float parsed = parameter->getValueForText(text);
            const auto label = floatParameter->paramID + " reads back " + text;
            require(parameter->getText(parsed, 0) == text, label.toRawUTF8());
        }
        for (const float normalised : { 0.0f, 1.0f, parameter->getDefaultValue() }) {
            const float parsed = parameter->getValueForText(parameter->getText(normalised, 0));
            const auto label = floatParameter->paramID + " round-trips its range ends and default";
            require(std::abs(parsed - normalised) < 1.0e-4f, label.toRawUTF8());
        }
    }
    const auto typed = [&p](const char* id, const char* text) {
        auto* parameter = p.parameters.getParameter(id);
        return parameter->convertFrom0to1(parameter->getValueForText(text));
    };
    const auto shown = [&p](const char* id, float value) {
        auto* parameter = p.parameters.getParameter(id);
        return parameter->getText(parameter->convertTo0to1(value), 0);
    };
    require(std::abs(typed("stereoize", "50") - 50.0f) < 0.01f && std::abs(typed("stereoize", "50%") - 50.0f) < 0.01f,
            "percent controls read 50 and 50% as 50%");
    require(std::abs(typed("frequency", "0.12k") - 120.0f) < 0.01f
                && std::abs(typed("frequency", "0.08 kHz") - 80.0f) < 0.01f,
            "frequency reads k and kHz as thousands");
    require(std::abs(typed("output_db", "-6 dB") + 6.0f) < 0.01f && std::abs(typed("upper_tilt", "+3.5") - 3.5f) < 0.01f,
            "gain controls read signed numbers with or without dB");
    require(shown("output_db", 3.0f) == "+3.0 dB" && shown("output_db", -0.0001f) == "0.0 dB"
                && shown("upper_tilt", -2.5f) == "-2.5 dB",
            "gain shows one decimal, a plus on boosts and never -0.0 dB");
    require(shown("frequency", 120.0f) == "120 Hz" && shown("width", 55.0f) == "55%" && shown("sub_drive", 100.0f) == "100%",
            "frequency and amounts show whole units");

    auto* outputDb = p.parameters.getParameter("output_db");
    require(outputDb != nullptr, "Output dB parameter missing");
    auto* upperWidthParam = p.parameters.getParameter("upper_width");
    auto* stereoizeParam = p.parameters.getParameter("stereoize");
    require(upperWidthParam != nullptr, "Upper-band width parameter missing");
    require(upperWidthParam->isAutomatable(), "Upper-band width parameter is automatable");
    require(std::abs(p.parameters.getRawParameterValue("upper_width")->load() - 100.0f) < 0.001f,
            "Upper-band width defaults to flat");
    require(stereoizeParam != nullptr, "Stereoize parameter missing");
    require(stereoizeParam->isAutomatable(), "Stereoize parameter is automatable");
    require(std::abs(p.parameters.getRawParameterValue("stereoize")->load()) < 0.001f,
            "Stereoize defaults off");
    require(outputDb->isAutomatable(), "Output dB parameter is automatable");
    require(std::abs(p.parameters.getRawParameterValue("output_db")->load()) < 0.001f,
            "Output dB defaults to unity");

    outputDb->setValueNotifyingHost(outputDb->convertTo0to1(-6.0f));
    upperWidthParam->setValueNotifyingHost(upperWidthParam->convertTo0to1(150.0f));
    stereoizeParam->setValueNotifyingHost(stereoizeParam->convertTo0to1(25.0f));
    juce::MemoryBlock savedState;
    p.getStateInformation(savedState);
    BassSpaceProcessor restored;
    restored.setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
    require(std::abs(restored.parameters.getRawParameterValue("output_db")->load() + 6.0f) < 0.01f,
            "Output dB is saved with the session");
    require(std::abs(restored.parameters.getRawParameterValue("upper_width")->load() - 150.0f) < 0.01f,
            "Upper-band width is saved with the session");
    require(std::abs(restored.parameters.getRawParameterValue("stereoize")->load() - 25.0f) < 0.01f,
            "Stereoize is saved with the session");

    auto legacyState = p.parameters.copyState();
    legacyState.removeChild(legacyState.getChildWithProperty("id", "upper_width"), nullptr);
    legacyState.removeChild(legacyState.getChildWithProperty("id", "stereoize"), nullptr);
    legacyState.removeChild(legacyState.getChildWithProperty("id", "sub_drive"), nullptr);
    legacyState.removeChild(legacyState.getChildWithProperty("id", "upper_tilt"), nullptr);
    legacyState.removeChild(legacyState.getChildWithProperty("id", "solo_low"), nullptr);
    legacyState.removeChild(legacyState.getChildWithProperty("id", "bypass"), nullptr);
    juce::MemoryBlock legacyStateData;
    juce::AudioProcessor::copyXmlToBinary(*legacyState.createXml(), legacyStateData);
    BassSpaceProcessor legacyRestored;
    legacyRestored.setStateInformation(legacyStateData.getData(), static_cast<int>(legacyStateData.getSize()));
    require(std::abs(legacyRestored.parameters.getRawParameterValue("upper_width")->load() - 100.0f) < 0.01f,
            "Legacy sessions default upper-band width to neutral");
    require(std::abs(legacyRestored.parameters.getRawParameterValue("stereoize")->load()) < 0.01f,
            "Legacy sessions default Stereoize off");
    require(std::abs(legacyRestored.parameters.getRawParameterValue("sub_drive")->load()) < 0.01f,
            "Legacy sessions default Sub Drive off");
    require(std::abs(legacyRestored.parameters.getRawParameterValue("upper_tilt")->load()) < 0.01f,
            "Legacy sessions default Upper Tilt flat");
    require(std::abs(legacyRestored.parameters.getRawParameterValue("solo_low")->load()) < 0.01f,
            "Legacy sessions default Solo Bass off");
    require(legacyRestored.getBypassParameter() != nullptr, "host bypass parameter is exposed");
    require(std::abs(legacyRestored.parameters.getRawParameterValue("bypass")->load()) < 0.01f,
            "Legacy sessions default Bypass off");

    const auto renderRms = [](float outputDbValue) {
        BassSpaceProcessor processor;
        processor.setRateAndBufferSizeDetails(48000, 512);
        processor.prepareToPlay(48000, 512);
        auto* parameter = processor.parameters.getParameter("output_db");
        require(parameter != nullptr, "Output dB parameter missing from render processor");
        parameter->setValueNotifyingHost(parameter->convertTo0to1(outputDbValue));

        juce::AudioBuffer<float> signal(2, 512);
        juce::MidiBuffer renderMidi;
        float sumSquares = 0.0f;
        for (int block = 0; block < 20; ++block) {
            for (int sample = 0; sample < signal.getNumSamples(); ++sample) {
                const auto position = static_cast<float>(block * signal.getNumSamples() + sample);
                const float value = 0.4f * std::sin(2.0f * 3.14159265f * 440.0f * position / 48000.0f);
                signal.setSample(0, sample, value);
                signal.setSample(1, sample, value);
            }
            processor.processBlock(signal, renderMidi);
            if (block == 19) {
                for (int sample = 0; sample < signal.getNumSamples(); ++sample) {
                    const float value = signal.getSample(0, sample);
                    sumSquares += value * value;
                }
            }
        }
        return std::sqrt(sumSquares / static_cast<float>(signal.getNumSamples()));
    };
    const float levelRatio = renderRms(-6.0f) / renderRms(0.0f);
    require(std::abs(levelRatio - 0.501187f) < 0.01f, "-6 dB output halves amplitude");

    BassSpaceProcessor stepProcessor;
    stepProcessor.setRateAndBufferSizeDetails(48000, 512);
    stepProcessor.prepareToPlay(48000, 512);
    auto* stepOutputDb = stepProcessor.parameters.getParameter("output_db");
    require(stepOutputDb != nullptr, "Output dB parameter missing from step processor");
    juce::AudioBuffer<float> steadySignal(2, 512);
    juce::MidiBuffer noMidi;
    for (int block = 0; block < 4; ++block) {
        steadySignal.clear();
        std::fill_n(steadySignal.getWritePointer(0), steadySignal.getNumSamples(), 0.1f);
        std::fill_n(steadySignal.getWritePointer(1), steadySignal.getNumSamples(), 0.1f);
        stepProcessor.processBlock(steadySignal, noMidi);
    }
    const float previousOutput = steadySignal.getSample(0, steadySignal.getNumSamples() - 1);
    stepOutputDb->setValueNotifyingHost(stepOutputDb->convertTo0to1(12.0f));
    steadySignal.clear();
    std::fill_n(steadySignal.getWritePointer(0), steadySignal.getNumSamples(), 0.1f);
    std::fill_n(steadySignal.getWritePointer(1), steadySignal.getNumSamples(), 0.1f);
    stepProcessor.processBlock(steadySignal, noMidi);
    require(std::abs(steadySignal.getSample(0, 0) - previousOutput) < 0.01f,
            "Output dB changes are smoothed");

    outputDb->setValueNotifyingHost(outputDb->convertTo0to1(0.0f));
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    pump();

    require(editor != nullptr, "Editor creation failed");
    require(editor->getWidth() == BassSpaceProcessor::defaultEditorWidth
                && editor->getHeight() == BassSpaceProcessor::defaultEditorHeight,
            "editor size match");
    require(editor->isResizable(), "editor is resizable");
    require(editor->getConstrainer() != nullptr && editor->getConstrainer()->getFixedAspectRatio() == 0.0,
            "editor has no fixed aspect ratio");
    auto* correlationReadout = dynamic_cast<juce::Label*>(findById(*editor, "correlationValue"));
    require(correlationReadout != nullptr && correlationReadout->getFont().getHeight() >= 24.0f
                && correlationReadout->getHeight() >= 32,
            "correlation readout stays large");
    require(findById(*editor, "undoButton") != nullptr, "undo button is visible");
    require(findById(*editor, "redoButton") != nullptr, "redo button is visible");
    require(findById(*editor, "compareA") != nullptr && findById(*editor, "compareB") != nullptr,
            "A/B buttons are visible");
    require(findById(*editor, "copyAtoB") != nullptr && findById(*editor, "copyBtoA") != nullptr,
            "A/B copy buttons are visible");
    require(findById(*editor, "bypassButton") != nullptr, "bypass button is visible");
    auto* presetButton = dynamic_cast<juce::Button*>(findById(*editor, "presetButton"));
    require(presetButton != nullptr && presetButton->getButtonText() == "Mono under 120 Hz",
            "factory preset name is visible");
    require(hasVisibleLabel(*editor, "Crossover"), "Crossover section label is visible");
    require(hasVisibleLabel(*editor, "Bass width"), "Bass width section label is visible");
    require(hasVisibleLabel(*editor, "Upper width"), "Upper-band width section label is visible");
    require(hasVisibleLabel(*editor, "Stereoize"), "Stereoize section label is visible");
    require(hasVisibleLabel(*editor, "Sub drive"), "Sub drive section label is visible");
    require(hasVisibleLabel(*editor, "Upper tilt"), "Upper tilt section label is visible");
    require(hasVisibleLabel(*editor, "Correlation"), "Correlation meter label is visible");
    require(hasVisibleLabel(*editor, "Output dB"), "Output dB section label is visible");
    require(hasVisibleLabel(*editor, "120 Hz"), "Crossover value includes frequency units");
    require(hasVisibleLabel(*editor, "0%"), "Stereo width value includes percent units");
    require(hasVisibleLabel(*editor, "0.0 dB"), "Output value includes decibel units");
    require(findSlider(*editor, "Crossover frequency") != nullptr, "Crossover control has an accessibility title");
    require(findSlider(*editor, "Bass stereo width") != nullptr, "Bass width control has an accessibility title");
    require(findSlider(*editor, "Upper-band width") != nullptr, "Upper-band width control has an accessibility title");
    require(findSlider(*editor, "Stereoize") != nullptr, "Stereoize control has an accessibility title");
    require(findSlider(*editor, "Sub harmonic drive") != nullptr, "Sub drive control has an accessibility title");
    require(findSlider(*editor, "Upper air tilt") != nullptr, "Upper tilt control has an accessibility title");
    require(findSlider(*editor, "Output level") != nullptr, "Output control has an accessibility title");
    checkLayout(*editor, "default size");

    struct Host : juce::AudioPlayHead {
        PositionInfo position;
        juce::Optional<PositionInfo> getPosition() const override { return position; }
    } host;
    p.setPlayHead(&host);
    juce::AudioBuffer<float> clockAudio(2, 256);
    juce::MidiBuffer clockMidi;
    clockAudio.clear();
    host.position.setIsPlaying(true);
    host.position.setBpm(140.0);
    host.position.setPpqPosition(-1.75);
    p.processBlock(clockAudio, clockMidi);
    require(std::abs(p.artworkPhase() - 0.25f) < 0.0001f, "SVG follows a negative host position");
    auto* stereoField = findById(*editor, "stereoField");
    require(stereoField != nullptr && !stereoField->getBounds().isEmpty(), "stereo field anchor is laid out");
    const auto onbeat = editor->createComponentSnapshot(stereoField->getBounds());
    host.position.setPpqPosition(22.875);
    p.processBlock(clockAudio, clockMidi);
    require(std::abs(p.artworkPhase() - 0.875f) < 0.0001f, "SVG follows a host seek");
    const auto offbeat = editor->createComponentSnapshot(stereoField->getBounds());
    require(changedPixels(onbeat, offbeat) > 30, "Native SVG pixels move with beat phase");
    host.position.setIsPlaying(false);
    host.position.setPpqPosition(0);
    p.processBlock(clockAudio, clockMidi);
    require(std::abs(p.artworkPhase() - 0.875f) < 0.0001f, "SVG holds phase when the host stops");
    p.setPlayHead(nullptr);
    p.processBlock(clockAudio, clockMidi);
    p.processBlock(clockAudio, clockMidi);
    require(p.artworkPhase() > 0.875f, "SVG advances on audio time without host transport");

    const juce::String outPath = argc > 1 ? juce::String(argv[1]) : "assets/native-ui.png";
    save(*editor, outPath);

    // Audio test: Verify collapse to mono for low frequencies when width is 0%
    p.parameters.getParameter(bass_space::params::kFrequency)->setValueNotifyingHost(
        p.parameters.getParameter(bass_space::params::kFrequency)->convertTo0to1(150.0f));
    p.parameters.getParameter(bass_space::params::kWidth)->setValueNotifyingHost(
        p.parameters.getParameter(bass_space::params::kWidth)->convertTo0to1(0.0f));

    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;

    // Run 6 blocks so the 60 Hz filter settles
    for (int block = 0; block < 6; ++block) {
        for (int i = 0; i < 512; ++i) {
            const float t = static_cast<float>(block * 512 + i) / 48000.0f;
            const float bassL = 0.5f * std::sin(2.0f * 3.14159f * 60.0f * t);
            const float bassR = -0.5f * std::sin(2.0f * 3.14159f * 60.0f * t); // pure out-of-phase low end
            audio.setSample(0, i, bassL);
            audio.setSample(1, i, bassR);
        }
        p.processBlock(audio, midi);
    }

    // After settling with 0% width, pure side low end is collapsed to mono (L - R == 0)
    float diffSum = 0.0f;
    for (int i = 0; i < 512; ++i) {
        diffSum += std::abs(audio.getSample(0, i) - audio.getSample(1, i));
    }
    const float avgDiff = diffSum / 512.0f;
    require(avgDiff < 0.05f, "Low end collapsed to mono");

    const auto renderUpperTone = [](float upperWidth, float stereoize, bool antiPhase) {
        BassSpaceProcessor processor;
        processor.prepareToPlay(48000, 512);
        auto setParameter = [&processor](const char* id, float value) {
            auto* parameter = processor.parameters.getParameter(id);
            require(parameter != nullptr, "Commercial stereo parameter missing");
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        setParameter("frequency", 150.0f);
        setParameter("width", 100.0f);
        setParameter("upper_width", upperWidth);
        setParameter("stereoize", stereoize);
        juce::AudioBuffer<float> signal(2, 512);
        juce::MidiBuffer renderMidi;
        for (int block = 0; block < 8; ++block) {
            for (int sample = 0; sample < signal.getNumSamples(); ++sample) {
                const float value = 0.5f * std::sin(2.0f * 3.14159265f * 5000.0f
                    * static_cast<float>(block * signal.getNumSamples() + sample) / 48000.0f);
                signal.setSample(0, sample, value);
                signal.setSample(1, sample, antiPhase ? -value : value);
            }
            processor.processBlock(signal, renderMidi);
        }
        float differenceEnergy = 0.0f;
        float monoEnergy = 0.0f;
        for (int sample = 0; sample < signal.getNumSamples(); ++sample) {
            const float leftValue = signal.getSample(0, sample);
            const float rightValue = signal.getSample(1, sample);
            const float difference = leftValue - rightValue;
            const float mono = 0.5f * (leftValue + rightValue);
            differenceEnergy += difference * difference;
            monoEnergy += mono * mono;
        }
        const float count = static_cast<float>(signal.getNumSamples());
        return std::array<float, 2>{std::sqrt(differenceEnergy / count), std::sqrt(monoEnergy / count)};
    };
    require(renderUpperTone(0.0f, 0.0f, true)[0] < 0.02f,
            "Upper-band width collapses only the high-frequency side signal");
    require(renderUpperTone(200.0f, 0.0f, true)[0] > 0.65f,
            "Upper-band width can preserve and expand high-frequency stereo content");
    const auto upperMono = renderUpperTone(100.0f, 0.0f, false);
    const auto stereoizedMono = renderUpperTone(100.0f, 100.0f, false);
    require(stereoizedMono[0] > 0.05f, "Stereoize creates upper-band width from a mono source");
    require(std::abs(stereoizedMono[1] - upperMono[1]) < 0.02f,
            "Stereoize keeps the mono fold of upper-band content stable");

    BassSpaceProcessor correlationProcessor;
    correlationProcessor.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> antiPhaseAudio(2, 512);
    juce::MidiBuffer correlationMidi;
    for (int block = 0; block < 8; ++block) {
        for (int sample = 0; sample < antiPhaseAudio.getNumSamples(); ++sample) {
            const float value = 0.5f * std::sin(2.0f * 3.14159265f * 5000.0f
                * static_cast<float>(block * antiPhaseAudio.getNumSamples() + sample) / 48000.0f);
            antiPhaseAudio.setSample(0, sample, value);
            antiPhaseAudio.setSample(1, sample, -value);
        }
        correlationProcessor.processBlock(antiPhaseAudio, correlationMidi);
    }
    std::unique_ptr<juce::AudioProcessorEditor> correlationEditor(correlationProcessor.createEditor());
    pump();
    require(hasVisibleLabel(*correlationEditor, "-1.00"),
            "Correlation meter reports an out-of-phase stereo signal");

    // Audio test: Verify mono input processing (DAW mono track)
    juce::AudioBuffer<float> monoAudio(1, 256);
    for (int i = 0; i < 256; ++i) {
        monoAudio.setSample(0, i, 0.4f);
    }
    p.processBlock(monoAudio, midi);
    for (int i = 0; i < 256; ++i) {
        require(std::isfinite(monoAudio.getSample(0, i)), "Mono output is finite");
    }

    // Audio test: Zero-channel/zero-sample safety
    juce::AudioBuffer<float> emptyAudio(0, 0);
    p.processBlock(emptyAudio, midi);

    // Enhancements test: Sub Drive, Upper Tilt, and Solo Bass
    {
        analog_master::core::StereoFieldShaper shaper;
        shaper.prepare(48000.0);
        shaper.setCrossoverFrequency(150.0);
        shaper.setWidth(0.0f);
        shaper.setSubDrive(0.8f);
        float outL = 0.0f, outR = 0.0f;
        for (int i = 0; i < 512; ++i) {
            const float in = 0.6f * std::sin(2.0f * 3.14159265f * 60.0f * static_cast<float>(i) / 48000.0f);
            shaper.processSample(in, in, outL, outR);
        }
        require(std::abs(outL - outR) < 1e-6f, "Sub drive remains 100% mono-locked");

        shaper.setSoloLow(true);
        float highEnergy = 0.0f;
        for (int i = 0; i < 1024; ++i) {
            const float in = 0.5f * std::sin(2.0f * 3.14159265f * 6000.0f * static_cast<float>(i) / 48000.0f);
            shaper.processSample(in, in, outL, outR);
            if (i >= 512) {
                highEnergy += outL * outL;
            }
        }
        require(highEnergy < 0.001f, "Solo Bass isolates the low band and rejects 6 kHz");

        shaper.setSoloLow(false);
        shaper.setUpperTiltDb(6.0f);
        float boostedEnergy = 0.0f;
        for (int i = 0; i < 1024; ++i) {
            const float in = 0.3f * std::sin(2.0f * 3.14159265f * 8000.0f * static_cast<float>(i) / 48000.0f);
            shaper.processSample(in, in, outL, outR);
            if (i >= 512) {
                boostedEnergy += outL * outL;
            }
        }
        shaper.setUpperTiltDb(-6.0f);
        float cutEnergy = 0.0f;
        for (int i = 0; i < 1024; ++i) {
            const float in = 0.3f * std::sin(2.0f * 3.14159265f * 8000.0f * static_cast<float>(i) / 48000.0f);
            shaper.processSample(in, in, outL, outR);
            if (i >= 512) {
                cutEnergy += outL * outL;
            }
        }
        require(boostedEnergy > cutEnergy * 4.0f, "Upper tilt boosts and attenuates high-band air");
    }

    // Processor ceiling guard + selectable LR8 / Elliptical parameter checks
    {
        BassSpaceProcessor hotProc;
        hotProc.setRateAndBufferSizeDetails(48000, 512);
        hotProc.prepareToPlay(48000, 512);
        auto* slopeParam = hotProc.parameters.getParameter("slope");
        auto* ellipticalParam = hotProc.parameters.getParameter("elliptical");
        require(slopeParam != nullptr && slopeParam->isAutomatable(), "LR8 slope parameter is exposed and automatable");
        require(ellipticalParam != nullptr && ellipticalParam->isAutomatable(), "Elliptical mode parameter is exposed and automatable");

        auto setHotParam = [&hotProc](const char* id, float plainValue) {
            auto* param = hotProc.parameters.getParameter(id);
            require(param != nullptr, "Hot test parameter missing");
            param->setValueNotifyingHost(param->convertTo0to1(plainValue));
        };
        setHotParam("upper_width", 200.0f);
        setHotParam("stereoize", 100.0f);
        setHotParam("upper_tilt", 6.0f);
        setHotParam("sub_drive", 80.0f);
        setHotParam("output_db", 12.0f);

        juce::AudioBuffer<float> hotAudio(2, 512);
        juce::MidiBuffer hotMidi;
        float maxOutPeak = 0.0f;
        for (int block = 0; block < 8; ++block) {
            for (int sample = 0; sample < 512; ++sample) {
                const float t = static_cast<float>(block * 512 + sample) / 48000.0f;
                const float mid = 0.55f * std::sin(2.0f * 3.14159265f * 95.0f * t)
                                + 0.45f * std::sin(2.0f * 3.14159265f * 5500.0f * t);
                const float side = 0.45f * std::sin(2.0f * 3.14159265f * 3800.0f * t + 0.5f);
                hotAudio.setSample(0, sample, mid + side);
                hotAudio.setSample(1, sample, mid - side);
            }
            hotProc.processBlock(hotAudio, hotMidi);
            for (int sample = 0; sample < 512; ++sample) {
                maxOutPeak = std::max({ maxOutPeak,
                                        std::abs(hotAudio.getSample(0, sample)),
                                        std::abs(hotAudio.getSample(1, sample)) });
            }
        }
        require(maxOutPeak <= 0.989f, "Processor ceiling guard caps hot boosted output at -0.1 dBFS");

        // Verify Elliptical parameter routes zero-phase Mid through BassSpaceProcessor
        BassSpaceProcessor ellipProc;
        ellipProc.setRateAndBufferSizeDetails(48000, 512);
        ellipProc.prepareToPlay(48000, 512);
        ellipProc.parameters.getParameter("elliptical")->setValueNotifyingHost(1.0f);
        ellipProc.parameters.getParameter("slope")->setValueNotifyingHost(1.0f);
        juce::AudioBuffer<float> ellipAudio(2, 512);
        for (int block = 0; block < 6; ++block) {
            for (int sample = 0; sample < 512; ++sample) {
                const float t = static_cast<float>(block * 512 + sample) / 48000.0f;
                const float kickMid = 0.35f * std::sin(2.0f * 3.14159265f * 120.0f * t);
                const float subSide = 0.35f * std::sin(2.0f * 3.14159265f * 60.0f * t);
                ellipAudio.setSample(0, sample, kickMid + subSide);
                ellipAudio.setSample(1, sample, kickMid - subSide);
            }
            ellipProc.processBlock(ellipAudio, hotMidi);
        }
        float midErr = 0.0f;
        float sideRes = 0.0f;
        for (int sample = 0; sample < 512; ++sample) {
            const float t = static_cast<float>(5 * 512 + sample) / 48000.0f;
            const float expectedMid = 0.35f * std::sin(2.0f * 3.14159265f * 120.0f * t);
            const float actualMid = 0.5f * (ellipAudio.getSample(0, sample) + ellipAudio.getSample(1, sample));
            const float actualSide = 0.5f * (ellipAudio.getSample(0, sample) - ellipAudio.getSample(1, sample));
            midErr = std::max(midErr, std::abs(actualMid - expectedMid));
            sideRes = std::max(sideRes, std::abs(actualSide));
        }
        require(midErr < 1e-4f, "Processor Elliptical mode keeps Mid phase-linear");
        require(sideRes < 0.005f, "Processor LR8 slope rejects 60 Hz Side below 120 Hz by > 48 dB");
    }

    editor->setSize(320, 180);
    require(editor->getWidth() >= BassSpaceProcessor::minEditorWidth
                && editor->getHeight() >= BassSpaceProcessor::minEditorHeight,
            "editor enforces a readable minimum size");
    checkLayout(*editor, "minimum size");
    editor->setSize(980, 700);
    require(editor->getWidth() == 980 && editor->getHeight() == 700, "editor accepts a free resize");
    auto* resizedReadout = dynamic_cast<juce::Label*>(findById(*editor, "correlationValue"));
    require(resizedReadout != nullptr && resizedReadout->getFont().getHeight() >= 24.0f,
            "correlation readout stays large when resized");

    {
        BassSpaceProcessor suite;
        suite.setRateAndBufferSizeDetails(48000, 512);
        suite.prepareToPlay(48000, 512);
        require(suite.getBypassParameter() != nullptr, "getBypassParameter is wired");
        require(suite.getBypassParameter() == suite.parameters.getParameter("bypass"),
                "bypass parameter is the host bypass");
        require(suite.getNumPrograms() == 9, "factory program count");
        const char* names[] = { "Mono under 120 Hz", "Mono under 80 Hz", "Mono under 200 Hz", "Flat",
                                "Wide upper only", "Stereoize", "Solo bass", "Sub punch", "Air tilt" };
        for (int i = 0; i < suite.getNumPrograms(); ++i) {
            require(suite.getProgramName(i) == names[i], "factory program name");
        }
        require(suite.getCurrentProgram() == 0, "default program is mono under 120 Hz");

        suite.setCurrentProgram(1);
        require(suite.getCurrentProgram() == 1, "current program follows the host");
        require(std::abs(suite.currentFrequency() - 80.0f) < 0.25f, "mono under 80 Hz sets the crossover");
        require(std::abs(suite.currentWidth()) < 0.25f, "mono under 80 Hz centers the bass");
        suite.setCurrentProgram(3);
        require(std::abs(suite.currentWidth() - 100.0f) < 0.25f, "Flat leaves bass width unchanged");
        suite.setCurrentProgram(4);
        require(std::abs(suite.currentUpperWidth() - 180.0f) < 0.25f, "Wide upper only widens the top band");
        require(std::abs(suite.currentWidth()) < 0.25f, "Wide upper only keeps the bass mono");
        suite.setCurrentProgram(6);
        require(suite.currentSoloLow(), "Solo bass program enables solo");
        require(suite.canUndo(), "loading a program is undoable");
        suite.undo();
        require(std::abs(suite.currentUpperWidth() - 180.0f) < 0.25f, "undo restores the previous program");
        require(!suite.currentSoloLow(), "undo clears Solo bass");
        suite.redo();
        require(suite.currentSoloLow(), "redo returns to Solo bass");

        auto* width = suite.parameters.getParameter("width");
        require(width != nullptr, "width parameter exists");
        width->beginChangeGesture();
        width->setValueNotifyingHost(width->convertTo0to1(40.0f));
        width->setValueNotifyingHost(width->convertTo0to1(80.0f));
        width->endChangeGesture();
        require(std::abs(suite.currentWidth() - 80.0f) < 0.25f, "gesture applies the final width");
        width->beginChangeGesture();
        width->setValueNotifyingHost(width->convertTo0to1(20.0f));
        width->endChangeGesture();
        suite.undo();
        require(std::abs(suite.currentWidth() - 80.0f) < 0.25f, "one undo reverts one gesture, not the samples inside it");
        suite.undo();
        require(std::abs(suite.currentWidth()) < 0.25f, "second undo restores the program width");

        width->beginChangeGesture();
        width->setValueNotifyingHost(width->convertTo0to1(55.0f));
        width->endChangeGesture();
        std::unique_ptr<juce::AudioProcessorEditor> suiteEditor(suite.createEditor());
        pump();
        auto* undoButton = dynamic_cast<juce::Button*>(findById(*suiteEditor, "undoButton"));
        require(undoButton != nullptr && undoButton->isEnabled(), "undo button enables after an edit");
        undoButton->triggerClick();
        pump();
        require(std::abs(suite.currentWidth()) < 0.25f, "undo button restores the parameter");
        width->beginChangeGesture();
        width->setValueNotifyingHost(width->convertTo0to1(55.0f));
        width->endChangeGesture();
        require(suiteEditor->keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)),
                "Ctrl or Cmd+Z is handled");
        require(std::abs(suite.currentWidth()) < 0.25f, "keyboard undo restores the parameter");
        require(suiteEditor->keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier
                                                            | juce::ModifierKeys::shiftModifier, 0)),
                "Shift+Ctrl or Cmd+Z is handled");
        require(std::abs(suite.currentWidth() - 55.0f) < 0.25f, "keyboard redo restores the parameter");

        suite.selectCompareSlot(1);
        require(suite.activeCompareSlot() == 1, "A/B toggle moves to B");
        require(std::abs(suite.currentWidth()) < 0.25f, "slot B still holds the earlier state");
        width->setValueNotifyingHost(width->convertTo0to1(15.0f));
        suite.selectCompareSlot(0);
        require(suite.activeCompareSlot() == 0, "A/B toggle returns to A");
        require(std::abs(suite.currentWidth() - 55.0f) < 0.25f, "slot A kept its width");
        suite.selectCompareSlot(1);
        require(std::abs(suite.currentWidth() - 15.0f) < 0.25f, "slot B kept its width");
        suite.copyCompareSlot(0, 1);
        require(std::abs(suite.currentWidth() - 55.0f) < 0.25f, "copy A to B updates the active B slot");
        suite.selectCompareSlot(0);
        require(std::abs(suite.currentWidth() - 55.0f) < 0.25f, "copy leaves A unchanged");
        suite.undo();
        suite.undo();
        require(suite.activeCompareSlot() == 1, "undo restores the compare slot");
        require(std::abs(suite.currentWidth() - 15.0f) < 0.25f, "undo restores the slot that copy replaced");

        suiteEditor->setSize(1020, 680);
        juce::MemoryBlock suiteState;
        suite.getStateInformation(suiteState);
        BassSpaceProcessor reopened;
        reopened.setStateInformation(suiteState.getData(), static_cast<int>(suiteState.getSize()));
        std::unique_ptr<juce::AudioProcessorEditor> reopenedEditor(reopened.createEditor());
        require(reopenedEditor->getWidth() == 1020 && reopenedEditor->getHeight() == 680,
                "editor size is saved with the session");
        require(reopened.activeCompareSlot() == suite.activeCompareSlot(), "compare slot is saved with the session");
        require(std::abs(reopened.currentWidth() - suite.currentWidth()) < 0.25f,
                "active parameters are saved with the compare slots");
        const int other = reopened.activeCompareSlot() == 0 ? 1 : 0;
        reopened.selectCompareSlot(other);
        require(std::abs(reopened.currentWidth() - 55.0f) < 0.25f, "the other compare slot is saved");

        auto presetDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("openBassSpace-preset-tests");
        presetDir.deleteRecursively();
        suite.setUserPresetDirectory(presetDir);
        auto& library = suite.presetLibrary();
        require(library.directory() == presetDir, "user presets use the requested directory");
        require(!library.saveUser("Flat", suite.currentParameterValues()), "factory names cannot be overwritten");
        require(!library.deleteUser("Wide upper only"), "factory presets cannot be deleted");
        require(library.isFactoryName("Solo bass"), "factory names stay recognised");
        require(library.saveUser("My mono", suite.currentParameterValues()), "user preset saves");
        require(library.filtered("my").size() == 1, "search finds the user preset");
        require(library.filtered("upper").size() == 1, "search finds a factory preset");
        require(library.setFavorite("Flat", true, true), "factory preset can be a favorite");
        require(library.setFavorite("My mono", false, true), "user preset can be a favorite");
        require(library.renameUser("My mono", "My wide"), "user preset renames");
        require(library.filtered("My mono").empty(), "renamed preset leaves its old name");
        library.rescan();
        bool factoryFavorite = false;
        bool userFavorite = false;
        for (const auto& entry : library.factory()) {
            if (entry.name == "Flat") {
                factoryFavorite = entry.favorite;
            }
        }
        for (const auto& entry : library.user()) {
            if (entry.name == "My wide") {
                userFavorite = entry.favorite;
            }
        }
        require(factoryFavorite && userFavorite, "favorites survive a rescan");
        alanmarcero::suite::PresetEntry custom;
        custom.name = "My wide";
        custom.factory = false;
        custom.values = suite.currentParameterValues();
        for (auto& value : custom.values) {
            if (value.first == "width") {
                value.second = 125.0f;
            }
        }
        suite.loadPreset(custom);
        require(std::abs(suite.currentWidth() - 125.0f) < 0.25f, "loading a user preset applies its values");
        require(suite.currentPresetName() == "My wide", "loading a user preset updates the name");
        require(library.deleteUser("My wide"), "user preset deletes");
        require(library.user().empty(), "deleted user preset is gone");
        require(library.factory().size() == 9, "factory bank remains after user preset edits");

        auto* bypass = suite.getBypassParameter();
        auto* output = suite.parameters.getParameter("output_db");
        auto* frequency = suite.parameters.getParameter("frequency");
        require(output != nullptr && frequency != nullptr, "bypass render parameters exist");
        frequency->setValueNotifyingHost(frequency->convertTo0to1(150.0f));
        width->setValueNotifyingHost(width->convertTo0to1(0.0f));
        output->setValueNotifyingHost(output->convertTo0to1(12.0f));
        auto* solo = suite.parameters.getParameter("solo_low");
        require(solo != nullptr, "solo parameter exists");
        solo->setValueNotifyingHost(0.0f);
        juce::AudioBuffer<float> bypassAudio(2, 512);
        juce::MidiBuffer bypassMidi;
        const auto fillBypass = [&bypassAudio](int block) {
            for (int sample = 0; sample < bypassAudio.getNumSamples(); ++sample) {
                const float value = 0.5f * std::sin(2.0f * 3.14159265f * 60.0f
                    * static_cast<float>(block * bypassAudio.getNumSamples() + sample) / 48000.0f);
                bypassAudio.setSample(0, sample, value);
                bypassAudio.setSample(1, sample, -value);
            }
        };
        for (int block = 0; block < 10; ++block) {
            fillBypass(block);
            suite.processBlock(bypassAudio, bypassMidi);
        }
        const float beforeBypass = bypassAudio.getSample(0, bypassAudio.getNumSamples() - 1);
        bypass->setValueNotifyingHost(1.0f);
        fillBypass(10);
        suite.processBlock(bypassAudio, bypassMidi);
        require(std::abs(bypassAudio.getSample(0, 0) - beforeBypass) < 0.05f,
                "engaging bypass does not jump");
        float maxStep = 0.0f;
        for (int sample = 1; sample < bypassAudio.getNumSamples(); ++sample) {
            maxStep = std::max(maxStep, std::abs(bypassAudio.getSample(0, sample) - bypassAudio.getSample(0, sample - 1)));
        }
        require(maxStep < 0.05f, "bypass crossfade has no click");
        for (int block = 11; block < 40; ++block) {
            fillBypass(block);
            suite.processBlock(bypassAudio, bypassMidi);
        }
        float settledError = 0.0f;
        for (int sample = 0; sample < bypassAudio.getNumSamples(); ++sample) {
            const float value = 0.5f * std::sin(2.0f * 3.14159265f * 60.0f
                * static_cast<float>(39 * bypassAudio.getNumSamples() + sample) / 48000.0f);
            settledError = std::max(settledError, std::abs(bypassAudio.getSample(0, sample) - value));
            settledError = std::max(settledError, std::abs(bypassAudio.getSample(1, sample) + value));
        }
        require(settledError < 0.02f, "settled bypass matches the dry input, including output trim");
        const float beforeReturn = bypassAudio.getSample(0, bypassAudio.getNumSamples() - 1);
        bypass->setValueNotifyingHost(0.0f);
        fillBypass(40);
        suite.processBlock(bypassAudio, bypassMidi);
        require(std::abs(bypassAudio.getSample(0, 0) - beforeReturn) < 0.05f,
                "leaving bypass does not jump");
    }

    {
        BassSpaceProcessor clicked;
        std::unique_ptr<juce::AudioProcessorEditor> clickedEditor(clicked.createEditor());
        auto* bypassButton = dynamic_cast<juce::Button*>(findById(*clickedEditor, "bypassButton"));
        require(bypassButton != nullptr && !clicked.currentBypass(), "bypass starts off");
        bypassButton->triggerClick();
        pump();
        require(clicked.currentBypass(), "clicking Bypass bypasses the plug-in");
        bypassButton->triggerClick();
        pump();
        require(!clicked.currentBypass(), "a second Bypass click turns bypass off");
    }

    {
        BassSpaceProcessor entry;
        std::unique_ptr<juce::AudioProcessorEditor> entryEditor(entry.createEditor());
        auto* crossover = findSlider(*entryEditor, "Crossover frequency");
        auto* width = findSlider(*entryEditor, "Bass stereo width");
        require(crossover != nullptr && width != nullptr, "dials for typed entry exist");

        const auto type = [](juce::Slider& slider, const juce::String& text) {
            auto* box = valueBoxOf(slider);
            require(box->isEditableOnSingleClick(), "a single click opens a value for typing");
            box->showEditor();
            require(box->getCurrentTextEditor() != nullptr, "clicking a value opens a text field");
            box->getCurrentTextEditor()->setText(text, false);
            box->hideEditor(false);
        };
        type(*crossover, "80");
        require(std::abs(entry.currentFrequency() - 80.0f) < 0.01f, "typing 80 sets the crossover to 80 Hz");
        type(*crossover, "0.2 kHz");
        require(std::abs(entry.currentFrequency() - 200.0f) < 0.01f, "typing 0.2 kHz sets the crossover to 200 Hz");
        entry.undo();
        require(std::abs(entry.currentFrequency() - 80.0f) < 0.01f, "undo reverts one typed value");

        const auto dragUp = [](juce::Slider& slider, float pixels, juce::ModifierKeys modifiers) {
            const auto source = juce::Desktop::getInstance().getMainMouseSource();
            const auto now = juce::Time::getCurrentTime();
            const juce::Point<float> start(20.0f, 20.0f);
            const auto event = [&](juce::Point<float> at) {
                return juce::MouseEvent(source, at, modifiers, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                        &slider, &slider, now, start, now, 1, true);
            };
            const double before = slider.valueToProportionOfLength(slider.getValue());
            slider.mouseDown(event(start));
            slider.mouseDrag(event(start.translated(0.0f, -pixels)));
            slider.mouseUp(event(start.translated(0.0f, -pixels)));
            return slider.valueToProportionOfLength(slider.getValue()) - before;
        };
        const juce::ModifierKeys leftButton(juce::ModifierKeys::leftButtonModifier);
        const double normalDrag = dragUp(*width, 40.0f, leftButton);
        const double fineDrag = dragUp(*width, 40.0f, leftButton.withFlags(juce::ModifierKeys::shiftModifier));
        require(normalDrag > 0.1 && std::abs(fineDrag / normalDrag - 0.1) < 0.02, "Shift-drag moves a tenth as far");

        require(width->getWantsKeyboardFocus() && findById(*entryEditor, "bypassButton")->getWantsKeyboardFocus(),
                "dials and buttons take keyboard focus");
        const float beforeKey = entry.currentWidth();
        require(width->keyPressed(juce::KeyPress(juce::KeyPress::upKey)) && entry.currentWidth() > beforeKey,
                "the up arrow steps a focused dial");
        entry.undo();
        require(std::abs(entry.currentWidth() - beforeKey) < 0.01f, "undo reverts one arrow-key step");
    }

    {
        BassSpaceProcessor modes;
        std::unique_ptr<juce::AudioProcessorEditor> modesEditor(modes.createEditor());
        auto* slopeButton = dynamic_cast<juce::Button*>(findById(*modesEditor, "slopeButton"));
        auto* sideOnlyButton = dynamic_cast<juce::Button*>(findById(*modesEditor, "sideOnlyButton"));
        auto* slope = modes.parameters.getParameter("slope");
        auto* sideOnly = modes.parameters.getParameter("elliptical");
        require(slopeButton != nullptr && slopeButton->getButtonText() == "SLOPE 24 dB",
                "the slope button names the current 24 dB slope");
        slopeButton->triggerClick();
        pump();
        require(slopeButton->getButtonText() == "SLOPE 48 dB" && slope->getCurrentValueAsText() == "48 dB",
                "the slope button and the host name the same 48 dB slope");
        require(slope->getName(64) == "Slope" && sideOnly->getName(64) == "Side-only split"
                    && sideOnlyButton != nullptr && sideOnlyButton->getButtonText() == "SIDE-ONLY SPLIT",
                "the host and the editor use one name for each mode");
    }

    {
        BassSpaceProcessor field;
        std::unique_ptr<juce::AudioProcessorEditor> fieldEditor(field.createEditor());
        auto* anchor = findById(*fieldEditor, "stereoField");
        require(anchor != nullptr, "stereo field is laid out");
        const auto wingsArea = anchor->getBounds().withTrimmedBottom(40);
        const auto monoBass = fieldEditor->createComponentSnapshot(wingsArea);
        field.setCurrentProgram(3);
        const auto flat = fieldEditor->createComponentSnapshot(wingsArea);
        require(changedPixels(monoBass, flat) > 30, "the stereo field draws the bass band, so Flat differs from mono bass");
    }

    std::cout << "openBassSpace v" << JucePlugin_VersionString
              << " checks passed and screenshot saved to " << outPath << "\n";
    return 0;
}

#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

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
    return std::any_of(component.getChildren().begin(), component.getChildren().end(), [&text](auto* child) {
        auto* label = dynamic_cast<juce::Label*>(child);
        return label != nullptr && label->getText() == text && label->isVisible()
            && !label->getBounds().isEmpty();
    });
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

static bool hasSliderTitle(juce::Component& component, const juce::String& title) {
    return std::any_of(component.getChildren().begin(), component.getChildren().end(), [&title](auto* child) {
        auto* slider = dynamic_cast<juce::Slider*>(child);
        return slider != nullptr && slider->getTitle() == title;
    });
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    BassSpaceProcessor p;
    p.setRateAndBufferSizeDetails(48000, 256);
    p.prepareToPlay(48000, 256);

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
    require(hasSliderTitle(*editor, "Crossover frequency"), "Crossover control has an accessibility title");
    require(hasSliderTitle(*editor, "Bass stereo width"), "Bass width control has an accessibility title");
    require(hasSliderTitle(*editor, "Upper-band width"), "Upper-band width control has an accessibility title");
    require(hasSliderTitle(*editor, "Stereoize"), "Stereoize control has an accessibility title");
    require(hasSliderTitle(*editor, "Sub harmonic drive"), "Sub drive control has an accessibility title");
    require(hasSliderTitle(*editor, "Upper air tilt"), "Upper tilt control has an accessibility title");
    require(hasSliderTitle(*editor, "Output level"), "Output control has an accessibility title");

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
    int changed = 0;
    for (int y = 0; y < onbeat.getHeight(); ++y)
        for (int x = 0; x < onbeat.getWidth(); ++x)
            if (onbeat.getPixelAt(x, y) != offbeat.getPixelAt(x, y)) ++changed;
    require(changed > 30, "Native SVG pixels move with beat phase");
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

    editor->setSize(320, 180);
    require(editor->getWidth() >= BassSpaceProcessor::minEditorWidth
                && editor->getHeight() >= BassSpaceProcessor::minEditorHeight,
            "editor enforces a readable minimum size");
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

    std::cout << "openBassSpace v" << JucePlugin_VersionString
              << " checks passed and screenshot saved to " << outPath << "\n";
    return 0;
}

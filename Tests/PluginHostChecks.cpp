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
    require(editor->getWidth() == 760 && editor->getHeight() == 480, "editor size match");
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
    const auto onbeat = editor->createComponentSnapshot(juce::Rectangle<int>(292, 140, 175, 77));
    host.position.setPpqPosition(22.875);
    p.processBlock(clockAudio, clockMidi);
    require(std::abs(p.artworkPhase() - 0.875f) < 0.0001f, "SVG follows a host seek");
    const auto offbeat = editor->createComponentSnapshot(juce::Rectangle<int>(292, 140, 175, 77));
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

    std::cout << "openBassSpace v" << JucePlugin_VersionString
              << " checks passed and screenshot saved to " << outPath << "\n";
    return 0;
}

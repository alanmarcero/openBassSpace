#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
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
    f.deleteFile();
    juce::FileOutputStream out(f);
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
    require(outputDb->isAutomatable(), "Output dB parameter is automatable");
    require(std::abs(p.parameters.getRawParameterValue("output_db")->load()) < 0.001f,
            "Output dB defaults to unity");

    outputDb->setValueNotifyingHost(outputDb->convertTo0to1(-6.0f));
    juce::MemoryBlock savedState;
    p.getStateInformation(savedState);
    BassSpaceProcessor restored;
    restored.setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
    require(std::abs(restored.parameters.getRawParameterValue("output_db")->load() + 6.0f) < 0.01f,
            "Output dB is saved with the session");

    const auto renderRms = [](float outputDbValue) {
        BassSpaceProcessor processor;
        processor.setRateAndBufferSizeDetails(48000, 512);
        processor.prepareToPlay(48000, 512);
        auto* parameter = processor.parameters.getParameter("output_db");
        require(parameter != nullptr, "Output dB parameter missing from render processor");
        parameter->setValueNotifyingHost(parameter->convertTo0to1(outputDbValue));

        juce::AudioBuffer<float> signal(2, 512);
        juce::MidiBuffer noMidi;
        float sumSquares = 0.0f;
        for (int block = 0; block < 20; ++block) {
            for (int sample = 0; sample < signal.getNumSamples(); ++sample) {
                const auto position = static_cast<float>(block * signal.getNumSamples() + sample);
                const float value = 0.4f * std::sin(2.0f * 3.14159265f * 440.0f * position / 48000.0f);
                signal.setSample(0, sample, value);
                signal.setSample(1, sample, value);
            }
            processor.processBlock(signal, noMidi);
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
    require(hasVisibleLabel(*editor, "Stereo width"), "Stereo width section label is visible");
    require(hasVisibleLabel(*editor, "Output dB"), "Output dB section label is visible");
    require(hasVisibleLabel(*editor, "120 Hz"), "Crossover value includes frequency units");
    require(hasVisibleLabel(*editor, "0%"), "Stereo width value includes percent units");
    require(hasVisibleLabel(*editor, "0.0 dB"), "Output value includes decibel units");
    require(hasSliderTitle(*editor, "Crossover frequency"), "Crossover control has an accessibility title");
    require(hasSliderTitle(*editor, "Stereo width"), "Width control has an accessibility title");
    require(hasSliderTitle(*editor, "Output level"), "Output control has an accessibility title");

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

    std::cout << "openBassSpace v" << JucePlugin_VersionString
              << " checks passed and screenshot saved to " << outPath << "\n";
    return 0;
}

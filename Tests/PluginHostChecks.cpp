#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
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

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    BassSpaceProcessor p;
    p.setRateAndBufferSizeDetails(48000, 256);
    p.prepareToPlay(48000, 256);

    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    pump();

    require(editor != nullptr, "Editor creation failed");
    require(editor->getWidth() == 560 && editor->getHeight() == 360, "editor size match");

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

    std::cout << "openBassSpace v2.0.0 checks passed and screenshot saved to " << outPath << "\n";
    return 0;
}

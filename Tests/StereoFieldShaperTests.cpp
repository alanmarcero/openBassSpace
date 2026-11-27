#include "StereoFieldShaper.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <numbers>
#include <vector>

using namespace analog_master::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("StereoFieldShaper collapses low frequencies to mono when width is 0", "[stereo]") {
    const double fs = 48000.0;
    StereoFieldShaper shaper;
    shaper.prepare(fs);
    shaper.setCrossoverFrequency(200.0);
    shaper.setWidth(0.0f); // 100% Mono for low end

    // Generate a 50 Hz out-of-phase low frequency tone: L = sin, R = -sin (pure side component)
    const double freq = 50.0;
    const int numSamples = 2000;
    std::vector<float> left(numSamples);
    std::vector<float> right(numSamples);

    for (int i = 0; i < numSamples; ++i) {
        float val = static_cast<float>(std::sin(2.0 * std::numbers::pi * freq * i / fs));
        left[i] = val;
        right[i] = -val;
    }

    shaper.processBlock(left.data(), right.data(), numSamples);

    // After settling, since width is 0 and input had pure side (mid = 0),
    // the output low end should be suppressed to near zero.
    float residualEnergy = 0.0f;
    for (int i = 1000; i < numSamples; ++i) {
        residualEnergy += std::abs(left[i] - right[i]);
    }
    float avgDiff = residualEnergy / 1000.0f;

    // Difference between L and R in the low band should be essentially zero (mono)
    REQUIRE(avgDiff < 0.05f);
}

TEST_CASE("StereoFieldShaper preserves high frequencies above crossover", "[stereo]") {
    const double fs = 48000.0;
    StereoFieldShaper shaper;
    shaper.prepare(fs);
    shaper.setCrossoverFrequency(100.0);
    shaper.setWidth(0.0f); // Low-end mono

    // Generate high frequency 5000 Hz tone: L = 1, R = 0 (high frequency stereo difference)
    const double freq = 5000.0;
    const int numSamples = 2000;
    std::vector<float> left(numSamples);
    std::vector<float> right(numSamples);

    for (int i = 0; i < numSamples; ++i) {
        float val = static_cast<float>(std::sin(2.0 * std::numbers::pi * freq * i / fs));
        left[i] = val;
        right[i] = 0.0f;
    }

    shaper.processBlock(left.data(), right.data(), numSamples);

    // At 5000 Hz (way above 100 Hz), left should still have full signal, right should remain near 0
    float leftPeak = 0.0f;
    float rightPeak = 0.0f;
    for (int i = 1500; i < numSamples; ++i) {
        leftPeak = std::max(leftPeak, std::abs(left[i]));
        rightPeak = std::max(rightPeak, std::abs(right[i]));
    }

    REQUIRE(leftPeak > 0.9f);
    REQUIRE(rightPeak < 0.05f);
}

TEST_CASE("StereoFieldShaper passthrough when width is 1.0", "[stereo]") {
    const double fs = 48000.0;
    StereoFieldShaper shaper;
    shaper.prepare(fs);
    shaper.setCrossoverFrequency(150.0);
    shaper.setWidth(1.0f); // Unity width

    // Crossover LR4 sums to flat allpass unity magnitude
    const double freq = 150.0;
    const int numSamples = 2000;
    std::vector<float> left(numSamples);
    std::vector<float> right(numSamples);

    for (int i = 0; i < numSamples; ++i) {
        left[i] = static_cast<float>(std::sin(2.0 * std::numbers::pi * freq * i / fs));
        right[i] = left[i];
    }

    shaper.processBlock(left.data(), right.data(), numSamples);

    // Output amplitude should remain 1.0
    float peak = 0.0f;
    for (int i = 1500; i < numSamples; ++i) {
        peak = std::max(peak, std::abs(left[i]));
    }
    REQUIRE_THAT(peak, WithinAbs(1.0f, 0.02f));
}

TEST_CASE("StereoFieldShaper enhances stereo width when width is 2.0", "[stereo]") {
    const double fs = 48000.0;
    StereoFieldShaper shaper;
    shaper.prepare(fs);
    shaper.setCrossoverFrequency(200.0);
    shaper.setWidth(2.0f); // 200% Enhanced stereo

    const double freq = 60.0;
    const int numSamples = 2000;
    std::vector<float> left(numSamples);
    std::vector<float> right(numSamples);

    for (int i = 0; i < numSamples; ++i) {
        float val = static_cast<float>(std::sin(2.0 * std::numbers::pi * freq * i / fs));
        left[i] = val * 0.5f;
        right[i] = -val * 0.5f; // Pure side signal with amplitude 0.5
    }

    shaper.processBlock(left.data(), right.data(), numSamples);

    // With width 2.0, side component is doubled: amplitude should reach ~1.0
    float peak = 0.0f;
    for (int i = 1000; i < numSamples; ++i) {
        peak = std::max(peak, std::abs(left[i]));
    }
    REQUIRE(peak > 0.90f);
}

TEST_CASE("StereoFieldShaper solo low band mutes high frequencies", "[stereo]") {
    const double fs = 48000.0;
    StereoFieldShaper shaper;
    shaper.prepare(fs);
    shaper.setCrossoverFrequency(150.0);
    shaper.setSoloLow(true);

    const int numSamples = 2000;
    std::vector<float> left(numSamples);
    std::vector<float> right(numSamples);

    // 5000 Hz high frequency tone
    for (int i = 0; i < numSamples; ++i) {
        float val = static_cast<float>(std::sin(2.0 * std::numbers::pi * 5000.0 * i / fs));
        left[i] = val;
        right[i] = val;
    }

    shaper.processBlock(left.data(), right.data(), numSamples);

    // Under Solo Low, high frequencies must be heavily attenuated (> 40 dB cut)
    float highPeak = 0.0f;
    for (int i = 1000; i < numSamples; ++i) {
        highPeak = std::max(highPeak, std::abs(left[i]));
    }
    REQUIRE(highPeak < 0.01f);
}

TEST_CASE("StereoFieldShaper keeps mono low frequencies centered when width is 2.0", "[stereo]") {
    const double fs = 48000.0;
    StereoFieldShaper shaper;
    shaper.prepare(fs);
    shaper.setCrossoverFrequency(150.0);
    shaper.setWidth(2.0f); // 200% Enhanced stereo

    const double freq = 40.0;
    const int numSamples = 8000;
    std::vector<float> left(numSamples);
    std::vector<float> right(numSamples);

    // Pure mono / center-panned 40 Hz sub-bass (L == R)
    for (int i = 0; i < numSamples; ++i) {
        float val = static_cast<float>(std::sin(2.0 * std::numbers::pi * freq * i / fs));
        left[i] = val;
        right[i] = val;
    }

    shaper.processBlock(left.data(), right.data(), numSamples);

    float maxStereoDiff = 0.0f;
    float maxMonoPeak = 0.0f;
    for (int i = 4000; i < numSamples; ++i) {
        maxStereoDiff = std::max(maxStereoDiff, std::abs(left[i] - right[i]));
        maxMonoPeak = std::max(maxMonoPeak, std::abs(0.5f * (left[i] + right[i])));
    }

    // A centered bass must stay centered.
    REQUIRE(maxStereoDiff < 1e-4f);
    REQUIRE_THAT(maxMonoPeak, WithinAbs(1.0f, 0.02f));
}

TEST_CASE("StereoFieldShaper independently narrows the upper band", "[stereo][imager]") {
    StereoFieldShaper shaper;
    shaper.prepare(48000.0);
    shaper.setCrossoverFrequency(150.0);
    shaper.setWidth(1.0f);
    shaper.setUpperWidth(0.0f);

    std::vector<float> left(4096);
    std::vector<float> right(4096);
    for (size_t sample = 0; sample < left.size(); ++sample) {
        const float value = static_cast<float>(std::sin(2.0 * std::numbers::pi * 5000.0 * sample / 48000.0));
        left[sample] = value;
        right[sample] = -value;
    }
    shaper.processBlock(left.data(), right.data(), static_cast<int>(left.size()));

    float residual = 0.0f;
    for (size_t sample = 1024; sample < left.size(); ++sample) {
        residual += std::abs(left[sample] - right[sample]);
    }
    REQUIRE(residual / 3072.0f < 0.05f);
}

TEST_CASE("StereoFieldShaper stereoize adds upper side energy without changing the mono fold", "[stereo][imager]") {
    StereoFieldShaper control;
    StereoFieldShaper widened;
    control.prepare(48000.0);
    widened.prepare(48000.0);
    control.setCrossoverFrequency(150.0);
    widened.setCrossoverFrequency(150.0);
    control.setWidth(1.0f);
    widened.setWidth(1.0f);
    widened.setStereoizeAmount(1.0f);

    std::vector<float> controlLeft(4096);
    std::vector<float> controlRight(4096);
    std::vector<float> wideLeft(4096);
    std::vector<float> wideRight(4096);
    for (size_t sample = 0; sample < controlLeft.size(); ++sample) {
        const float value = static_cast<float>(std::sin(2.0 * std::numbers::pi * 5000.0 * sample / 48000.0));
        controlLeft[sample] = value;
        controlRight[sample] = value;
        wideLeft[sample] = value;
        wideRight[sample] = value;
    }
    control.processBlock(controlLeft.data(), controlRight.data(), static_cast<int>(controlLeft.size()));
    widened.processBlock(wideLeft.data(), wideRight.data(), static_cast<int>(wideLeft.size()));

    float sideEnergy = 0.0f;
    float foldError = 0.0f;
    for (size_t sample = 1024; sample < wideLeft.size(); ++sample) {
        sideEnergy += std::abs(wideLeft[sample] - wideRight[sample]);
        const float controlMono = 0.5f * (controlLeft[sample] + controlRight[sample]);
        const float wideMono = 0.5f * (wideLeft[sample] + wideRight[sample]);
        foldError = std::max(foldError, std::abs(controlMono - wideMono));
    }
    REQUIRE(sideEnergy / 3072.0f > 0.05f);
    REQUIRE(foldError < 0.01f);
}

TEST_CASE("StereoFieldShaper correlation handles mono, polarity inversion, and silence", "[stereo][imager]") {
    REQUIRE_THAT(StereoFieldShaper::correlationCoefficient(2.0, 2.0, 2.0), WithinAbs(1.0, 1e-12));
    REQUIRE_THAT(StereoFieldShaper::correlationCoefficient(-2.0, 2.0, 2.0), WithinAbs(-1.0, 1e-12));
    REQUIRE_THAT(StereoFieldShaper::correlationCoefficient(0.0, 0.0, 0.0), WithinAbs(0.0, 1e-12));
}

TEST_CASE("Correlation meter reads silence as no value", "[stereo][meter]") {
    CorrelationMeter meter;
    meter.prepare(48000.0);
    for (int i = 0; i < 4800; ++i) {
        meter.process(0.0f, 0.0f);
    }
    REQUIRE(std::isnan(meter.fullBand()));
    REQUIRE(std::isnan(meter.lowBand()));
}

TEST_CASE("Correlation meter confirms mono bass under an out-of-phase top", "[stereo][meter]") {
    const double fs = 48000.0;
    CorrelationMeter meter;
    meter.prepare(fs);
    meter.setCrossover(120.0, CrossoverSlope::LR4_24dB);
    for (int i = 0; i < 48000; ++i) {
        const double t = static_cast<double>(i) / fs;
        const auto bass = static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi * 60.0 * t));
        const auto top = static_cast<float>(0.7 * std::sin(2.0 * std::numbers::pi * 3000.0 * t));
        meter.process(bass + top, bass - top);
    }
    REQUIRE(meter.fullBand() < -0.2f);
    REQUIRE(meter.lowBand() > 0.98f);
}

TEST_CASE("Correlation meter smooths over a few hundred milliseconds and then reads silence", "[stereo][meter]") {
    const double fs = 48000.0;
    CorrelationMeter meter;
    meter.prepare(fs);
    const auto run = [&meter, fs](double seconds, float level, float rightSign) {
        for (int i = 0; i < static_cast<int>(seconds * fs); ++i) {
            const auto tone = static_cast<float>(level * std::sin(2.0 * std::numbers::pi * 1000.0 * i / fs));
            meter.process(tone, rightSign * tone);
        }
    };
    run(1.0, 0.5f, 1.0f);
    REQUIRE(meter.fullBand() > 0.99f);
    run(0.05, 0.5f, -1.0f);
    REQUIRE(meter.fullBand() > 0.5f);
    run(1.5, 0.5f, -1.0f);
    REQUIRE(meter.fullBand() < -0.95f);
    run(0.1, 0.0f, 1.0f);
    REQUIRE_FALSE(std::isnan(meter.fullBand()));
    run(0.9, 0.0f, 1.0f);
    REQUIRE(std::isnan(meter.fullBand()));
}

namespace {

double measureToneMagnitude(const float* samples, int startSample, int count, double freqHz, double sampleRate) {
    double realSum = 0.0;
    double imagSum = 0.0;
    const double omega = 2.0 * std::numbers::pi * freqHz / sampleRate;
    for (int idx = 0; idx < count; ++idx) {
        const double angle = omega * static_cast<double>(idx);
        const double sample = static_cast<double>(samples[startSample + idx]);
        realSum += sample * std::cos(angle);
        imagSum -= sample * std::sin(angle);
    }
    return 2.0 * std::sqrt(realSum * realSum + imagSum * imagSum) / static_cast<double>(count);
}

} // namespace

TEST_CASE("Sub drive synthesizes octave-down f/2 sub-harmonic and warm even harmonics", "[stereo][subdrive]") {
    constexpr double fs = 48000.0;
    constexpr double fundamentalHz = 90.0;
    constexpr int numSamples = 16384;

    StereoFieldShaper shaper;
    shaper.prepare(fs);
    shaper.setCrossoverFrequency(130.0);
    shaper.setWidth(0.0f);
    shaper.setSubDrive(0.85f);

    std::vector<float> left(numSamples);
    std::vector<float> right(numSamples);
    for (int idx = 0; idx < numSamples; ++idx) {
        const float value = static_cast<float>(0.55 * std::sin(2.0 * std::numbers::pi * fundamentalHz * idx / fs));
        left[idx] = value;
        right[idx] = value;
    }

    shaper.processBlock(left.data(), right.data(), numSamples);

    const double subOctaveMag = measureToneMagnitude(left.data(), 4096, 12000, fundamentalHz * 0.5, fs);
    const double secondHarmonicMag = measureToneMagnitude(left.data(), 4096, 12000, fundamentalHz * 2.0, fs);

    float maxDiff = 0.0f;
    for (int idx = 4096; idx < numSamples; ++idx) {
        maxDiff = std::max(maxDiff, std::abs(left[idx] - right[idx]));
    }

    REQUIRE(subOctaveMag > 0.025);
    REQUIRE(secondHarmonicMag > 0.005);
    REQUIRE(maxDiff < 1e-6f);

    // Ensure sub-octave + DC states decay cleanly without thump when bass stops and Solo Low switches to 6 kHz
    StereoFieldShaper noteOffCheck;
    noteOffCheck.prepare(fs);
    noteOffCheck.setCrossoverFrequency(150.0);
    noteOffCheck.setWidth(0.0f);
    noteOffCheck.setSubDrive(0.8f);
    float outL = 0.0f;
    float outR = 0.0f;
    for (int idx = 0; idx < 512; ++idx) {
        const float bass = static_cast<float>(0.6 * std::sin(2.0 * std::numbers::pi * 60.0 * idx / fs));
        noteOffCheck.processSample(bass, bass, outL, outR);
    }
    noteOffCheck.setSoloLow(true);
    float highEnergy = 0.0f;
    for (int idx = 0; idx < 1024; ++idx) {
        const float high = static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi * 6000.0 * idx / fs));
        noteOffCheck.processSample(high, high, outL, outR);
        if (idx >= 512) {
            highEnergy += outL * outL;
        }
    }
    REQUIRE(highEnergy < 0.001f);
}

TEST_CASE("Negative sub level turns the bass band down and leaves the upper band alone", "[stereo][subdrive]") {
    constexpr double fs = 48000.0;
    constexpr int numSamples = 16384;

    const auto bassAndHighMagnitude = [&](float subLevel, bool sideOnlySplit) {
        StereoFieldShaper shaper;
        shaper.prepare(fs);
        shaper.setCrossoverFrequency(130.0);
        shaper.setWidth(1.0f);
        shaper.setEllipticalMode(sideOnlySplit);
        shaper.setSubDrive(subLevel);

        std::vector<float> left(numSamples);
        std::vector<float> right(numSamples);
        for (int idx = 0; idx < numSamples; ++idx) {
            const double bass = std::sin(2.0 * std::numbers::pi * 60.0 * idx / fs);
            const double high = std::sin(2.0 * std::numbers::pi * 4000.0 * idx / fs);
            left[idx] = right[idx] = static_cast<float>(0.3 * bass + 0.2 * high);
        }
        shaper.processBlock(left.data(), right.data(), numSamples);
        return std::pair{ measureToneMagnitude(left.data(), 4096, 12000, 60.0, fs),
                          measureToneMagnitude(left.data(), 4096, 12000, 4000.0, fs) };
    };

    for (const bool sideOnlySplit : { false, true }) {
        const auto [flatBass, flatHigh] = bassAndHighMagnitude(0.0f, sideOnlySplit);
        const auto [halfBass, halfHigh] = bassAndHighMagnitude(-0.5f, sideOnlySplit);
        const auto [mutedBass, mutedHigh] = bassAndHighMagnitude(-1.0f, sideOnlySplit);

        REQUIRE(halfBass < flatBass * 0.65);
        REQUIRE(halfBass > flatBass * 0.35);
        // The 24 dB/oct high band still passes about 4% of a 60 Hz tone, so full cut is not absolute silence.
        REQUIRE(mutedBass < flatBass * 0.06);
        REQUIRE_THAT(halfHigh, WithinAbs(flatHigh, flatHigh * 0.05));
        REQUIRE_THAT(mutedHigh, WithinAbs(flatHigh, flatHigh * 0.05));
    }
}

TEST_CASE("Sub drive contains upper midrange harmonic leakage and preserves crossover phase", "[stereo][subdrive]") {
    constexpr double fs = 48000.0;
    constexpr double fundamentalHz = 110.0;
    constexpr int numSamples = 12288;

    StereoFieldShaper shaper;
    shaper.prepare(fs);
    shaper.setCrossoverFrequency(130.0);
    shaper.setWidth(1.0f);
    shaper.setSubDrive(1.0f);

    std::vector<float> left(numSamples);
    std::vector<float> right(numSamples);
    for (int idx = 0; idx < numSamples; ++idx) {
        const float value = static_cast<float>(0.75 * std::sin(2.0 * std::numbers::pi * fundamentalHz * idx / fs));
        left[idx] = value;
        right[idx] = value;
    }

    shaper.processBlock(left.data(), right.data(), numSamples);

    // Midrange odd harmonics (5f + 7f + 9f = 550 + 770 + 990 Hz, > 4x crossover) must be contained by post-LPF
    const double h5 = measureToneMagnitude(left.data(), 4096, 8000, fundamentalHz * 5.0, fs);
    const double h7 = measureToneMagnitude(left.data(), 4096, 8000, fundamentalHz * 7.0, fs);
    const double h9 = measureToneMagnitude(left.data(), 4096, 8000, fundamentalHz * 9.0, fs);
    const double midrangeLeakage = h5 + h7 + h9;

    // Also verify ADAA suppresses inharmonic aliasing on high-frequency bass harmonic content
    StereoFieldShaper xoverCheck;
    xoverCheck.prepare(fs);
    xoverCheck.setCrossoverFrequency(130.0);
    xoverCheck.setWidth(1.0f);
    xoverCheck.setSubDrive(0.8f);
    std::vector<float> xL(8192);
    std::vector<float> xR(8192);
    for (int idx = 0; idx < 8192; ++idx) {
        const float value = static_cast<float>(0.4 * std::sin(2.0 * std::numbers::pi * 130.0 * idx / fs));
        xL[idx] = value;
        xR[idx] = value;
    }
    xoverCheck.processBlock(xL.data(), xR.data(), 8192);
    const double xoverOutMag = measureToneMagnitude(xL.data(), 2048, 6000, 130.0, fs);

    REQUIRE(midrangeLeakage < 0.018);
    REQUIRE(xoverOutMag > 0.34);
}

TEST_CASE("Low-band mono sum preserves RMS power on phase-offset chorus bass", "[stereo][monosum]") {
    constexpr double fs = 48000.0;
    constexpr double bassHz = 70.0;
    constexpr double phaseOffsetRad = 110.0 * std::numbers::pi / 180.0;
    constexpr int numSamples = 16000;

    StereoFieldShaper shaper;
    shaper.prepare(fs);
    shaper.setCrossoverFrequency(160.0);
    shaper.setWidth(0.0f);

    std::vector<float> left(numSamples);
    std::vector<float> right(numSamples);
    double inPowerSum = 0.0;
    for (int idx = 0; idx < numSamples; ++idx) {
        const double phase = 2.0 * std::numbers::pi * bassHz * idx / fs;
        left[idx] = static_cast<float>(0.5 * std::sin(phase));
        right[idx] = static_cast<float>(0.5 * std::sin(phase + phaseOffsetRad));
        if (idx >= 8000) {
            inPowerSum += 0.5 * (static_cast<double>(left[idx]) * left[idx]
                                 + static_cast<double>(right[idx]) * right[idx]);
        }
    }

    shaper.processBlock(left.data(), right.data(), numSamples);

    double outPowerSum = 0.0;
    float maxDiff = 0.0f;
    for (int idx = 8000; idx < numSamples; ++idx) {
        outPowerSum += static_cast<double>(left[idx]) * left[idx];
        maxDiff = std::max(maxDiff, std::abs(left[idx] - right[idx]));
    }

    const double rmsIn = std::sqrt(inPowerSum / 8000.0);
    const double rmsOut = std::sqrt(outPowerSum / 8000.0);

    // Without energy compensation, 110 deg offset drops amplitude to cos(55 deg) = 0.574 (-4.8 dB)
    REQUIRE(rmsOut / rmsIn > 0.92);
    REQUIRE(maxDiff < 0.035f);
}

TEST_CASE("Elliptical mode keeps Mid channel 100% phase-linear while high-passing Side", "[stereo][elliptical]") {
    constexpr double fs = 48000.0;
    constexpr int numSamples = 4096;

    StereoFieldShaper shaper;
    shaper.prepare(fs);
    shaper.setCrossoverFrequency(120.0);
    shaper.setWidth(0.0f);
    shaper.setUpperWidth(1.0f);
    shaper.setEllipticalMode(true);

    std::vector<float> dryMid(numSamples);
    std::vector<float> left(numSamples);
    std::vector<float> right(numSamples);
    for (int idx = 0; idx < numSamples; ++idx) {
        const double t = static_cast<double>(idx) / fs;
        // Multi-harmonic punchy kick-like Mid waveform spanning across 120 Hz crossover
        const float kickMid = static_cast<float>(
            0.35 * std::sin(2.0 * std::numbers::pi * 55.0 * t)
            + 0.25 * std::sin(2.0 * std::numbers::pi * 120.0 * t + 0.4)
            + 0.15 * std::sin(2.0 * std::numbers::pi * 260.0 * t - 0.7));
        const float bassSide = static_cast<float>(0.25 * std::sin(2.0 * std::numbers::pi * 50.0 * t));
        dryMid[idx] = kickMid;
        left[idx] = kickMid + bassSide;
        right[idx] = kickMid - bassSide;
    }

    shaper.processBlock(left.data(), right.data(), numSamples);

    float maxMidError = 0.0f;
    float maxResidualBassSide = 0.0f;
    for (int idx = 2048; idx < numSamples; ++idx) {
        const float outMid = 0.5f * (left[idx] + right[idx]);
        const float outSide = 0.5f * (left[idx] - right[idx]);
        maxMidError = std::max(maxMidError, std::abs(outMid - dryMid[idx]));
        maxResidualBassSide = std::max(maxResidualBassSide, std::abs(outSide));
    }

    REQUIRE(maxMidError < 1e-5f);
    REQUIRE(maxResidualBassSide < 0.025f);
}

TEST_CASE("Frequency-progressive bass width keeps sub-35 Hz centered even at 200% width", "[stereo][widthcurve]") {
    constexpr double fs = 48000.0;
    constexpr int numSamples = 12000;

    auto measureSideGain = [&](double freqHz) {
        StereoFieldShaper shaper;
        shaper.prepare(fs);
        shaper.setCrossoverFrequency(200.0);
        shaper.setWidth(2.0f);

        std::vector<float> left(numSamples);
        std::vector<float> right(numSamples);
        for (int idx = 0; idx < numSamples; ++idx) {
            const float s = static_cast<float>(0.35 * std::sin(2.0 * std::numbers::pi * freqHz * idx / fs));
            left[idx] = s;
            right[idx] = -s;
        }
        shaper.processBlock(left.data(), right.data(), numSamples);

        float sidePeak = 0.0f;
        for (int idx = 6000; idx < numSamples; ++idx) {
            sidePeak = std::max(sidePeak, 0.5f * std::abs(left[idx] - right[idx]));
        }
        return sidePeak / 0.35f;
    };

    const float upperBassGain = measureSideGain(110.0);
    const float subRumbleGain = measureSideGain(22.0);

    REQUIRE(upperBassGain > 1.65f);
    REQUIRE(subRumbleGain < 0.85f);
}

TEST_CASE("Selectable LR8 48 dB/oct crossover provides steep stopband rejection and flat unity sum", "[stereo][lr8]") {
    constexpr double fs = 48000.0;
    constexpr int numSamples = 8192;

    // 1. Stopband check one octave below 160 Hz cutoff (80 Hz pure side)
    StereoFieldShaper shaper;
    shaper.prepare(fs);
    shaper.setCrossoverFrequency(160.0);
    shaper.setCrossoverSlope(CrossoverSlope::LR8_48dB);
    shaper.setWidth(0.0f);

    std::vector<float> left(numSamples);
    std::vector<float> right(numSamples);
    for (int idx = 0; idx < numSamples; ++idx) {
        const float s = static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi * 80.0 * idx / fs));
        left[idx] = s;
        right[idx] = -s;
    }
    shaper.processBlock(left.data(), right.data(), numSamples);

    float residualPeak = 0.0f;
    for (int idx = 4096; idx < numSamples; ++idx) {
        residualPeak = std::max(residualPeak, 0.5f * std::abs(left[idx] - right[idx]));
    }
    // LR4 at 0.5*fc is 0.5 / 17 = 0.0294; LR8 at 0.5*fc is 0.5 / 257 = 0.00195
    REQUIRE(residualPeak < 0.005f);

    // 2. Unity magnitude sum at crossoverHz when width = 1.0
    StereoFieldShaper flatCheck;
    flatCheck.prepare(fs);
    flatCheck.setCrossoverFrequency(160.0);
    flatCheck.setCrossoverSlope(CrossoverSlope::LR8_48dB);
    flatCheck.setWidth(1.0f);
    for (int idx = 0; idx < numSamples; ++idx) {
        const float m = static_cast<float>(0.8 * std::sin(2.0 * std::numbers::pi * 160.0 * idx / fs));
        left[idx] = m;
        right[idx] = m;
    }
    flatCheck.processBlock(left.data(), right.data(), numSamples);
    float monoPeak = 0.0f;
    for (int idx = 4096; idx < numSamples; ++idx) {
        monoPeak = std::max(monoPeak, std::abs(left[idx]));
    }
    REQUIRE_THAT(monoPeak, WithinAbs(0.8f, 0.015f));
}

TEST_CASE("Multi-ms Schroeder/Velvet stereoizer is sample-rate invariant and high-passed", "[stereo][stereoize]") {
    // 1. Multi-millisecond tail (> 2 ms after 1 ms burst ends)
    {
        constexpr double fs = 48000.0;
        constexpr int numSamples = 2048;
        StereoFieldShaper shaper;
        shaper.prepare(fs);
        shaper.setCrossoverFrequency(150.0);
        shaper.setWidth(1.0f);
        shaper.setStereoizeAmount(1.0f);

        std::vector<float> left(numSamples, 0.0f);
        std::vector<float> right(numSamples, 0.0f);
        for (int idx = 0; idx < 48; ++idx) {
            const float win = 0.5f * (1.0f - std::cos(2.0f * std::numbers::pi_v<float> * idx / 47.0f));
            const float burst = win * std::sin(2.0f * std::numbers::pi_v<float> * 2500.0f * idx / 48000.0f);
            left[idx] = burst;
            right[idx] = burst;
        }
        shaper.processBlock(left.data(), right.data(), numSamples);

        float lateDiffusedPeak = 0.0f;
        // Check window 2.0 ms to 12.0 ms (samples 96..576) after burst starts
        for (int idx = 144; idx < 650; ++idx) {
            lateDiffusedPeak = std::max(lateDiffusedPeak, std::abs(left[idx] - right[idx]));
        }
        REQUIRE(lateDiffusedPeak > 0.02f);
    }

    // 2. Sample-rate invariance between 48 kHz and 96 kHz at 900 Hz, and low-frequency rejection
    auto measureSideRms = [](double fs, double freqHz) {
        const int total = static_cast<int>(fs * 0.15);
        const int start = total / 2;
        StereoFieldShaper shaper;
        shaper.prepare(fs);
        shaper.setCrossoverFrequency(150.0);
        shaper.setWidth(1.0f);
        shaper.setStereoizeAmount(1.0f);

        std::vector<float> left(total);
        std::vector<float> right(total);
        for (int idx = 0; idx < total; ++idx) {
            const float val = static_cast<float>(0.4 * std::sin(2.0 * std::numbers::pi * freqHz * idx / fs));
            left[idx] = val;
            right[idx] = val;
        }
        shaper.processBlock(left.data(), right.data(), total);

        double sideSq = 0.0;
        for (int idx = start; idx < total; ++idx) {
            const double side = 0.5 * (static_cast<double>(left[idx]) - right[idx]);
            sideSq += side * side;
        }
        return std::sqrt(sideSq / static_cast<double>(total - start));
    };

    const double side48k = measureSideRms(48000.0, 900.0);
    const double side96k = measureSideRms(96000.0, 900.0);
    const double bassLeak = measureSideRms(48000.0, 45.0);

    REQUIRE(side48k > 0.08);
    REQUIRE_THAT(side96k, WithinAbs(side48k, 0.03));
    REQUIRE(bassLeak < 0.01);
}

TEST_CASE("Upper tilt uses symmetrical TPT shelf smoothed in log-gain dB", "[stereo][tilt]") {
    constexpr double fs = 48000.0;
    constexpr int numSamples = 8192;

    // 1. Cascading +6 dB and -6 dB shelves produces flat reciprocal unity across pivot
    StereoFieldShaper boostShaper;
    StereoFieldShaper cutShaper;
    boostShaper.prepare(fs);
    cutShaper.prepare(fs);
    boostShaper.setCrossoverFrequency(80.0);
    cutShaper.setCrossoverFrequency(80.0);
    boostShaper.setWidth(1.0f);
    cutShaper.setWidth(1.0f);
    boostShaper.setUpperTiltDb(6.0f);
    cutShaper.setUpperTiltDb(-6.0f);

    std::vector<float> left(numSamples);
    std::vector<float> right(numSamples);
    for (int idx = 0; idx < numSamples; ++idx) {
        const float val = static_cast<float>(0.3 * std::sin(2.0 * std::numbers::pi * 2400.0 * idx / fs));
        left[idx] = val;
        right[idx] = val;
    }
    boostShaper.processBlock(left.data(), right.data(), numSamples);
    cutShaper.processBlock(left.data(), right.data(), numSamples);

    const double pivotMag = measureToneMagnitude(left.data(), 2048, 6000, 2400.0, fs);
    REQUIRE_THAT(pivotMag, WithinAbs(0.3, 0.008));

    // 2. Log-gain dB smoothing: sweeping -6 dB to +6 dB passes through ~0 dB (unity) at block midpoint
    StereoFieldShaper sweepShaper;
    sweepShaper.prepare(fs);
    sweepShaper.setCrossoverFrequency(80.0);
    sweepShaper.setWidth(1.0f);
    sweepShaper.setUpperTiltDb(-6.0f);

    constexpr int settleSamples = 2048;
    std::vector<float> sL(settleSamples);
    std::vector<float> sR(settleSamples);
    for (int idx = 0; idx < settleSamples; ++idx) {
        const float val = static_cast<float>(0.3 * std::sin(2.0 * std::numbers::pi * 12000.0 * idx / fs));
        sL[idx] = val;
        sR[idx] = val;
    }
    sweepShaper.processBlock(sL.data(), sR.data(), settleSamples);

    sweepShaper.setUpperTiltDb(6.0f);
    constexpr int rampSamples = 4096;
    std::vector<float> rL(rampSamples);
    std::vector<float> rR(rampSamples);
    for (int idx = 0; idx < rampSamples; ++idx) {
        const float val = static_cast<float>(0.3 * std::sin(2.0 * std::numbers::pi * 12000.0 * (settleSamples + idx) / fs));
        rL[idx] = val;
        rR[idx] = val;
    }
    sweepShaper.processBlock(rL.data(), rR.data(), rampSamples);

    // Measure local amplitude right around midpoint of the dB ramp (samples 1984..2112)
    const double midRampMag = measureToneMagnitude(rL.data(), 1984, 128, 12000.0, fs);
    // Log-gain midpoint is 0 dB -> 0.30; linear gain midpoint would be 1.248 * 0.30 = 0.374!
    REQUIRE_THAT(midRampMag, WithinAbs(0.30, 0.025));
}

TEST_CASE("LinkwitzRiley4 smooths TPT g continuously per sample during fast cutoff jumps", "[stereo][crossover]") {
    constexpr double fs = 48000.0;
    LinkwitzRiley4 xover;
    xover.prepare(fs);
    xover.setCrossoverFrequency(45.0);

    // Warm up filter state on a steady 220 Hz signal (in HP passband ~0.80 at 45 Hz cutoff)
    for (int idx = 0; idx < 2048; ++idx) {
        const float in = static_cast<float>(0.8 * std::sin(2.0 * std::numbers::pi * 220.0 * idx / fs));
        (void)xover.process(in);
    }

    // Jump cutoff from 45 Hz to 480 Hz: if g snaps immediately instead of smoothing per sample (~5 ms tau),
    // the 220 Hz HP output collapses into stopband (< 0.15) within 1.5 ms (72 samples), whereas per-sample
    // g smoothing is still gliding smoothly through ~100 Hz at 1.0-1.5 ms (HP peak > 0.50) and settles later.
    xover.setCrossoverFrequency(480.0);
    float earlyHpPeak = 0.0f;
    float settledHpPeak = 0.0f;
    for (int idx = 2048; idx < 4096; ++idx) {
        const float in = static_cast<float>(0.8 * std::sin(2.0 * std::numbers::pi * 220.0 * idx / fs));
        const float hp = std::abs(xover.process(in).high);
        if (idx >= 2096 && idx < 2168) {
            earlyHpPeak = std::max(earlyHpPeak, hp);
        }
        if (idx >= 3600) {
            settledHpPeak = std::max(settledHpPeak, hp);
        }
    }

    REQUIRE(earlyHpPeak > 0.45f);
    REQUIRE(settledHpPeak < 0.05f);
}

TEST_CASE("True-peak-safe M/S ceiling guard protects output at -0.1 dBFS under extreme boost", "[stereo][ceiling]") {
    constexpr double fs = 48000.0;
    constexpr int numSamples = 8192;

    StereoFieldShaper shaper;
    shaper.prepare(fs);
    shaper.setCrossoverFrequency(120.0);
    shaper.setWidth(1.5f);
    shaper.setUpperWidth(2.0f);
    shaper.setStereoizeAmount(1.0f);
    shaper.setSubDrive(0.8f);
    shaper.setUpperTiltDb(6.0f);

    std::vector<float> left(numSamples);
    std::vector<float> right(numSamples);
    for (int idx = 0; idx < numSamples; ++idx) {
        const double t = static_cast<double>(idx) / fs;
        const float mid = static_cast<float>(0.55 * std::sin(2.0 * std::numbers::pi * 95.0 * t)
                                             + 0.45 * std::sin(2.0 * std::numbers::pi * 6000.0 * t));
        const float side = static_cast<float>(0.55 * std::sin(2.0 * std::numbers::pi * 4200.0 * t + 0.7));
        left[idx] = mid + side;
        right[idx] = mid - side;
    }

    shaper.processBlock(left.data(), right.data(), numSamples);

    float maxSamplePeak = 0.0f;
    float maxTruePeak = 0.0f;
    for (int idx = 2; idx < numSamples - 1; ++idx) {
        REQUIRE(std::isfinite(left[idx]));
        REQUIRE(std::isfinite(right[idx]));
        maxSamplePeak = std::max({ maxSamplePeak, std::abs(left[idx]), std::abs(right[idx]) });

        // 4-point Catmull-Rom midpoint inter-sample peak check
        const float midInterpL = 0.5625f * (left[idx - 1] + left[idx]) - 0.0625f * (left[idx - 2] + left[idx + 1]);
        const float midInterpR = 0.5625f * (right[idx - 1] + right[idx]) - 0.0625f * (right[idx - 2] + right[idx + 1]);
        maxTruePeak = std::max({ maxTruePeak, std::abs(midInterpL), std::abs(midInterpR) });
    }

    // -0.1 dBFS = 0.988553f
    REQUIRE(maxSamplePeak <= 0.989f);
    REQUIRE(maxTruePeak <= 1.005f);
}

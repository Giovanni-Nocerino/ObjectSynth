#pragma once

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

// The Distortion module: drive gain -> tanh soft clipper -> make-up gain.
//
// Why it is built this way:
//  - Drive range: the 1..10 knob maps to 0..+36 dB of gain, linear in dB, so
//    it really goes from clean to almost square (a plain x1..x10 gain never
//    pushed a normal note hard into the curve).
//  - Make-up gain: squashing a waveform raises its energy, so louder output
//    with more drive. A table (built once, off the audio thread) holds the
//    gain that keeps the RMS of a sine at referenceLevel unchanged for every
//    drive: the Drive knob changes the harmonics, not the volume.
//  - 4x oversampling: the clipper creates harmonics far above Nyquist; at
//    the base rate they would fold back as inharmonic aliasing ("fizz").
//  - The dry path is delayed by the oversampling latency, so Dry/Wet and
//    the bypass (wet = 0) never comb-filter and the latency is constant
//    (reported to the host by the processor).
class Saturator final
{
public:
    // Typical sustained peak of one note at this stage:
    // SynthVoice::voiceLevel (0.25) x the default sustain (0.7).
    static constexpr float referenceLevel = 0.18f;
    static constexpr float maxDriveDecibels = 36.0f;

    Saturator()
    {
        // RMS-matching make-up gain, sampled over the whole knob range.
        for (std::size_t k = 0; k < compensation.size(); ++k)
        {
            const float knob = 1.0f + 9.0f * static_cast<float>(k)
                                          / static_cast<float>(compensation.size() - 1);
            const double gain = driveToGain(knob);
            double input = 0.0, output = 0.0;
            constexpr int steps = 512;
            for (int i = 0; i < steps; ++i)
            {
                const double x = referenceLevel
                    * std::sin(juce::MathConstants<double>::twoPi * i / steps);
                const double y = std::tanh(gain * x);
                input += x * x;
                output += y * y;
            }
            compensation[k] = static_cast<float>(std::sqrt(input / output));
        }
    }

    // Knob value (1..10) -> linear gain before the clipper (0..+36 dB).
    static float driveToGain(float knob) noexcept
    {
        return juce::Decibels::decibelsToGain(driveToDecibels(knob));
    }

    static float driveToDecibels(float knob) noexcept
    {
        return maxDriveDecibels * (juce::jlimit(1.0f, 10.0f, knob) - 1.0f) / 9.0f;
    }

    void prepare(const juce::dsp::ProcessSpec& spec, float initialDrive, float initialWet)
    {
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
            static_cast<std::size_t>(spec.numChannels), oversamplingOrder,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true,   // max quality
            true);  // integer latency, so it can be reported and compensated
        oversampling->initProcessing(static_cast<std::size_t>(spec.maximumBlockSize));

        mixer.prepare(spec);
        mixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);
        mixer.setWetLatency(oversampling->getLatencyInSamples());
        mixer.setWetMixProportion(juce::jlimit(0.0f, 1.0f, initialWet));
        mixer.reset(); // start at the initial mix, without a ramp

        drive.reset(spec.sampleRate * static_cast<double>(1 << oversamplingOrder), 0.02);
        drive.setCurrentAndTargetValue(juce::jlimit(1.0f, 10.0f, initialDrive));
        oversampling->reset();
    }

    void setDrive(float newDrive) noexcept
    {
        if (std::isfinite(newDrive))
            drive.setTargetValue(juce::jlimit(1.0f, 10.0f, newDrive));
    }

    void setMix(float wet) noexcept
    {
        mixer.setWetMixProportion(juce::jlimit(0.0f, 1.0f, wet));
    }

    int getLatencySamples() const noexcept
    {
        return oversampling != nullptr
            ? juce::roundToInt(oversampling->getLatencyInSamples()) : 0;
    }

    float makeUpGain(float knob) const noexcept
    {
        const float position = (juce::jlimit(1.0f, 10.0f, knob) - 1.0f) / 9.0f
                               * static_cast<float>(compensation.size() - 1);
        const auto index = std::min(static_cast<std::size_t>(position), compensation.size() - 2);
        const float fraction = position - static_cast<float>(index);
        return compensation[index] + fraction * (compensation[index + 1] - compensation[index]);
    }

    void process(juce::dsp::AudioBlock<float> block) noexcept
    {
        jassert(oversampling != nullptr);
        mixer.pushDrySamples(block);

        auto up = oversampling->processSamplesUp(block);
        const auto channels = up.getNumChannels();
        const auto samples = up.getNumSamples();

        if (! drive.isSmoothing())
        {
            const float knob = drive.getTargetValue();
            const float gain = driveToGain(knob);
            const float makeUp = makeUpGain(knob);
            for (std::size_t ch = 0; ch < channels; ++ch)
            {
                auto* data = up.getChannelPointer(ch);
                for (std::size_t i = 0; i < samples; ++i)
                    data[i] = std::tanh(gain * data[i]) * makeUp;
            }
        }
        else
        {
            // Drive is shared by all channels: step the smoother once per frame.
            for (std::size_t i = 0; i < samples; ++i)
            {
                const float knob = drive.getNextValue();
                const float gain = driveToGain(knob);
                const float makeUp = makeUpGain(knob);
                for (std::size_t ch = 0; ch < channels; ++ch)
                {
                    auto* data = up.getChannelPointer(ch);
                    data[i] = std::tanh(gain * data[i]) * makeUp;
                }
            }
        }

        oversampling->processSamplesDown(block);
        mixer.mixWetSamples(block);
    }

private:
    static constexpr std::size_t oversamplingOrder = 2; // 2^2 = 4x

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DryWetMixer<float> mixer {64}; // max wet latency in samples
    juce::SmoothedValue<float> drive {3.0f};
    std::array<float, 64> compensation {};
};

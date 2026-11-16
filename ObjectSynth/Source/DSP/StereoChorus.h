#pragma once

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>

// Stereo chorus with the same parameters (and the same delay formula) as
// juce::dsp::Chorus, which ObjectSynth used before. The difference: JUCE's
// chorus modulates every channel with ONE delay curve, so identical L/R
// inputs stay identical (mono). Here the right channel's LFO runs 90 degrees
// ahead of the left one, so the two delays differ and the image widens.
//
//   delay(ms) = max(1, centre + 20 * (0.5 * depth) * lfo),  lfo = sin(...)
//
// Feedback is not used by ObjectSynth and is not implemented.
class StereoChorus final
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = std::min<std::size_t>(2, static_cast<std::size_t>(spec.numChannels));

        delay.prepare(spec);
        delay.setMaximumDelayInSamples(static_cast<int>(
            std::ceil((maxModulationMs * 0.5 + maxCentreDelayMs) * sampleRate / 1000.0)) + 2);

        mixer.prepare(spec);
        mixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);

        depthSmoothed.reset(sampleRate, 0.05);
        centreSmoothed.reset(sampleRate, 0.005);
        reset();
    }

    void reset()
    {
        delay.reset();
        mixer.setWetMixProportion(mix);
        mixer.reset();
        depthSmoothed.setCurrentAndTargetValue(depth);
        centreSmoothed.setCurrentAndTargetValue(centreDelayMs);
        phase = 0.0;
    }

    void setRate(float hz) noexcept
    {
        if (std::isfinite(hz))
            rateHz = juce::jlimit(0.0f, 99.0f, hz);
    }

    void setDepth(float newDepth) noexcept
    {
        if (std::isfinite(newDepth))
        {
            depth = juce::jlimit(0.0f, 1.0f, newDepth);
            depthSmoothed.setTargetValue(depth);
        }
    }

    void setCentreDelay(float milliseconds) noexcept
    {
        if (std::isfinite(milliseconds))
        {
            centreDelayMs = juce::jlimit(1.0f, maxCentreDelayMs, milliseconds);
            centreSmoothed.setTargetValue(centreDelayMs);
        }
    }

    void setMix(float wet) noexcept
    {
        if (std::isfinite(wet))
        {
            mix = juce::jlimit(0.0f, 1.0f, wet);
            mixer.setWetMixProportion(mix);
        }
    }

    // Right-channel LFO offset in cycles (0.25 = 90 degrees).
    static constexpr double stereoPhaseOffset = 0.25;

    void process(juce::dsp::AudioBlock<float> block) noexcept
    {
        const auto channels = std::min(numChannels, block.getNumChannels());
        const auto samples = block.getNumSamples();
        mixer.pushDrySamples(block);

        const double increment = static_cast<double>(rateHz) / sampleRate;
        for (std::size_t i = 0; i < samples; ++i)
        {
            const float swing = maxModulationMs * 0.5f * depthSmoothed.getNextValue();
            const float centre = centreSmoothed.getNextValue();

            for (std::size_t ch = 0; ch < channels; ++ch)
            {
                const double channelPhase = phase + (ch == 1 ? stereoPhaseOffset : 0.0);
                const float lfo = static_cast<float>(
                    std::sin(juce::MathConstants<double>::twoPi * channelPhase));
                const float delayMs = juce::jmax(1.0f, centre + swing * lfo);

                auto* data = block.getChannelPointer(ch);
                delay.pushSample(static_cast<int>(ch), data[i]);
                data[i] = delay.popSample(static_cast<int>(ch),
                                          delayMs * static_cast<float>(sampleRate) / 1000.0f);
            }

            phase += increment;
            phase -= std::floor(phase);
        }

        mixer.mixWetSamples(block);
    }

private:
    static constexpr float maxCentreDelayMs = 100.0f;
    static constexpr float maxModulationMs = 20.0f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay;
    juce::dsp::DryWetMixer<float> mixer;
    juce::SmoothedValue<float> depthSmoothed {0.3f};
    juce::SmoothedValue<float> centreSmoothed {7.0f};

    double sampleRate = 48000.0;
    std::size_t numChannels = 2;
    double phase = 0.0;
    float rateHz = 0.8f, depth = 0.3f, centreDelayMs = 7.0f, mix = 0.4f;
};

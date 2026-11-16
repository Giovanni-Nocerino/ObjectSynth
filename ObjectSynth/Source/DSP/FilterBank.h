#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

class FilterBank
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        for (auto& filter : filters)
        {
            filter.prepare(spec);
            filter.setCutoffFrequency(800.0f);
            filter.setResonance(0.7071f);
            filter.reset();
        }

        using Type = juce::dsp::StateVariableTPTFilterType;
        filters[0].setType(Type::lowpass);
        filters[1].setType(Type::highpass);
        filters[2].setType(Type::bandpass);
        outputMix = 1.0f;
    }

    void setParameters(std::size_t index, float cutoff, float resonance) noexcept
    {
        jassert(index < filters.size());
        if (index >= filters.size()) return;
        filters[index].setCutoffFrequency(cutoff);
        filters[index].setResonance(resonance);
    }

    void setMix(float lp, float hp, float bp) noexcept { mix = {lp, hp, bp}; }
    void setOutputMix(float mixValue) noexcept
    {
        outputMix = juce::jlimit(0.0f, 1.0f, mixValue);
    }

    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        process(buffer, 0, buffer.getNumSamples());
    }

    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
    {
        jassert(startSample >= 0 && numSamples >= 0);
        jassert(startSample <= buffer.getNumSamples());
        jassert(numSamples <= buffer.getNumSamples() - startSample);

        if (startSample < 0 || numSamples < 0 ||
            startSample > buffer.getNumSamples() ||
            numSamples > buffer.getNumSamples() - startSample)
            return;

        const float total = mix[0] + mix[1] + mix[2];
        const float dryGain = juce::jmax(0.0f, 1.0f - total);
        const float normalization = juce::jmax(1.0f, total);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* samples = buffer.getWritePointer(channel, startSample);

            for (int i = 0; i < numSamples; ++i)
            {
                const float input = samples[i];
                const float lp = filters[0].processSample(channel, input);
                const float hp = filters[1].processSample(channel, input);
                const float bp = filters[2].processSample(channel, input);
                const float filtered = (dryGain * input + mix[0] * lp +
                                        mix[1] * hp + mix[2] * bp) / normalization;

                // Process the filters continuously, even during the bypass fade.
                samples[i] = outputMix >= 1.0f ? filtered :
                             outputMix <= 0.0f ? input :
                             input + outputMix * (filtered - input);
            }
        }

        for (auto& filter : filters)
            filter.snapToZero();
    }

private:
    std::array<juce::dsp::StateVariableTPTFilter<float>, 3> filters;
    std::array<float, 3> mix {1.0f, 0.0f, 0.0f};
    float outputMix = 1.0f;
};

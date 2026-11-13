#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

class FilterBank{

public:

    void prepare(const juce::dsp::ProcessSpec& spec){
        for (auto& filter : filters){
            filter.prepare(spec);
            filter.setCutoffFrequency(800.0f);
            filter.setResonance(0.7071f);
            filter.reset();
        }

        using Type = juce::dsp::StateVariableTPTFilterType;

        filters[0].setType(Type::lowpass);
        filters[1].setType(Type::highpass);
        filters[2].setType(Type::bandpass);
    }

    void setParameters(std::size_t index, float cutoff, float resonance) noexcept {
        jassert(index < filters.size());

        if (index >= filters.size())
            return;

        filters[index].setCutoffFrequency(cutoff);
        filters[index].setResonance(resonance);
    }

    void setMix(float lp, float hp, float bp) noexcept{
        mix = {lp, hp, bp};
    }

    void process(juce::AudioBuffer<float>& buffer) noexcept{

        const float total = mix[0] + mix[1] + mix[2];

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch){
            auto* samples = buffer.getWritePointer(ch);

            for (int n = 0; n < buffer.getNumSamples(); ++n){
                const float input = samples[n];

                const float lp = filters[0].processSample(ch, input);
                const float hp = filters[1].processSample(ch, input);
                const float bp = filters[2].processSample(ch, input);

                const float dryGain = juce::jmax(0.0f, 1.0f - total);
                const float normalization = juce::jmax(1.0f, total);

                samples[n] = (dryGain * input + mix[0]*lp + mix[1]*hp + mix[2]*bp) / normalization;
            }
        }

        for (auto& filter : filters)
            filter.snapToZero();
    }

private:
    std::array<juce::dsp::StateVariableTPTFilter<float>, 3> filters;
    std::array<float, 3> mix {1.0f, 0.0f, 0.0f};
};


#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "WavetableOscillator.h"

class SynthVoice final : public juce::SynthesiserVoice
{
public:
    SynthVoice() = default;

    bool canPlaySound(
        juce::SynthesiserSound* sound
    ) override;

    void startNote(
        int midiNoteNumber,
        float velocity,
        juce::SynthesiserSound* sound,
        int pitchWheelPosition
    ) override;

    void stopNote(
        float velocity,
        bool allowTailOff
    ) override;

    void pitchWheelMoved(int newValue) override;

    void controllerMoved(
        int controllerNumber,
        int newValue
    ) override;

    void renderNextBlock(
        juce::AudioBuffer<float>& outputBuffer,
        int startSample,
        int numSamples
    ) override;

    void setCurrentPlaybackSampleRate(
        double newSampleRate
    ) override;

    void setWaveform(Waveform newWaveform) noexcept{
        oscillator.setWaveform(newWaveform);
    }

    void setADSRParameters(const juce::ADSR::Parameters& newParameters) noexcept{
        pendingADSRParameters = newParameters;
    }

    void setVoiceFilterEnabled(bool enabled) noexcept
    {
        if (enabled && !voiceLowPassEnabled)
            voiceLowPass.reset();
        voiceLowPassEnabled = enabled;
        voiceFilterMix.setTargetValue(enabled ? 1.0f : 0.0f);
    }

    void setFilterEnvelopeParameters(const juce::ADSR::Parameters& parameters, float baseCutoff, float amountOctaves) noexcept {
        filterASDRParameters = parameters;
        voiceFilterBaseCutoff = baseCutoff;
        voiceFilterAmountOctaves = amountOctaves;
    }


private:
    WavetableOscillator oscillator;

    juce::ADSR envelope;
    juce::ADSR::Parameters pendingADSRParameters {0.01f, 0.10f, 0.70f, 0.20f};

    juce::ADSR filterEnvelope;
    juce::ADSR::Parameters filterASDRParameters {0.01f, 0.35f, 0.0f, 0.20f};
    float voiceFilterBaseCutoff = 800.0f;
    float voiceFilterAmountOctaves = 3.0f;

    juce::dsp::StateVariableTPTFilter<float> voiceLowPass;
    bool voiceLowPassEnabled = true;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> voiceFilterMix {1.0f};

    float noteGain = 0.0f;
};

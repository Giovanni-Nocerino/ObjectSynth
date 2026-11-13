
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
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


private:
    WavetableOscillator oscillator;

    juce::ADSR envelope;
    juce::ADSR::Parameters pendingADSRParameters {0.01f, 0.10f, 0.70f, 0.20f};
    float noteGain = 0.0f;
};

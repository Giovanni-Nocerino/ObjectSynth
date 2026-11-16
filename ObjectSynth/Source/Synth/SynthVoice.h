
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
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
        pendingWaveform = newWaveform;
    }

    void setCustomWaveform(const WavetableBank::Table* table) noexcept {
        pendingCustomTable = table;
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

    // Oscillator bypass is smoothed even for notes already playing.
    void setOscillatorEnabled(bool enabled) noexcept
    {
        oscillatorGate.setTargetValue(enabled ? 1.0f : 0.0f);
    }

    // Envelope and Unison bypass latch at the next Note On.
    void setAmplitudeEnvelopeEnabled(bool enabled) noexcept
    {
        pendingAmplitudeEnvelopeEnabled = enabled;
    }

    void setUnisonEnabled(bool enabled) noexcept
    {
        pendingUnisonEnabled = enabled;
    }

    void setUnisonParameters(int count, float detuneCents) noexcept {
        pendindUnisonCount = juce::jlimit(1, maxUnisonOscillators, count);
        if (std::isfinite(detuneCents)){
            pendingDetuneCents = juce::jlimit(0.0f, 25.0f, detuneCents);
        }
    }


private:

    Waveform pendingWaveform = Waveform::Sawtooth;
    const WavetableBank::Table* pendingCustomTable = nullptr;

    static constexpr int maxUnisonOscillators = 7;

    std::array<WavetableOscillator, maxUnisonOscillators> oscillators;
    int pendindUnisonCount = 1;
    int activeUnisonCount = 1;
    float pendingDetuneCents = 0.0f;
    bool pendingUnisonEnabled = true;
    bool pendingAmplitudeEnvelopeEnabled = true;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> oscillatorGate {1.0f};

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


#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include "WavetableOscillator.h"

class SynthVoice final : public juce::SynthesiserVoice
{
public:
    // Peak level of one full-velocity note (unison normalised): -12 dBFS.
    // Saturator::referenceLevel is matched to this value.
    static constexpr float voiceLevel = 0.25f;

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

    // setId identifies the immutable snapshot that owns `table`
    // (0 = none); see WavetableSnapshotStore.h.
    void setCustomWaveform(const WavetableBank::Table* table, std::uint64_t setId) noexcept {
        pendingCustomTable = table;
        pendingCustomSetId = setId;
    }

    // Id of the custom wavetable snapshot this voice may still read while
    // it sounds, or 0 if it reads none (idle, or a built-in waveform).
    std::uint64_t getCustomSetIdInUse() const noexcept {
        return isVoiceActive() ? latchedCustomSetId : 0;
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

    // Latched at Note On, like detune. spread: 0 = mono, 1 = outermost
    // oscillators hard left/right.
    void setUnisonParameters(int count, float detuneCents, float spread) noexcept {
        pendindUnisonCount = juce::jlimit(1, maxUnisonOscillators, count);
        if (std::isfinite(detuneCents)){
            pendingDetuneCents = juce::jlimit(0.0f, 25.0f, detuneCents);
        }
        if (std::isfinite(spread)){
            pendingSpread = juce::jlimit(0.0f, 1.0f, spread);
        }
    }


private:

    Waveform pendingWaveform = Waveform::Sawtooth;
    const WavetableBank::Table* pendingCustomTable = nullptr;
    std::uint64_t pendingCustomSetId = 0;
    std::uint64_t latchedCustomSetId = 0;

    static constexpr int maxUnisonOscillators = 7;

    std::array<WavetableOscillator, maxUnisonOscillators> oscillators;
    int pendindUnisonCount = 1;
    int activeUnisonCount = 1;
    float unisonNormalisation = 1.0f;
    float pendingDetuneCents = 0.0f;
    float pendingSpread = 0.5f;
    // Per-oscillator stereo gains, set at Note On (centre = 1 / 1).
    std::array<float, maxUnisonOscillators> panLeft {}, panRight {};
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

    // Velocity gain, ramped when a stolen voice changes velocity.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> noteGain {0.0f};

    // Set by a hard stopNote() while still sounding. If startNote() follows
    // (voice stealing), the new note continues from the current level.
    bool retriggerFromCurrentLevel = false;
};


#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include "LiveADSR.h"
#include "WavetableOscillator.h"

// One polyphonic voice: up to 7 unison oscillators -> voice low-pass with
// its filter envelope -> amplitude envelope.
//
// Every setting reaches notes that are already sounding ("live"): the
// processor calls the setters below once per host block (filter envelope:
// once per 32-sample control tick), and each change is applied with a short
// transition instead of a jump, because a jump in the waveform is a click:
//  - waveform / custom frame / wavetable: 10 ms crossfade between the old
//    and new table at the same phase (WavetableOscillator);
//  - unison voices: added oscillators fade in, removed ones fade out, and
//    the sqrt(N) loudness normalisation glides;
//  - detune: frequencies change, phases continue (no discontinuity);
//  - spread: per-oscillator pan gains glide;
//  - ADSR times / sustain: LiveADSR continues from the current level.
class SynthVoice final : public juce::SynthesiserVoice
{
public:
    // Peak level of one full-velocity note (unison normalised): -12 dBFS.
    // Saturator::referenceLevel is matched to this value.
    static constexpr float voiceLevel = 0.25f;
    // Length of every live transition (crossfades, unison fades, pan glides).
    static constexpr double transitionSeconds = 0.01;

    SynthVoice() = default;

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound* sound, int pitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newValue) override;
    void controllerMoved(int controllerNumber, int newValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample, int numSamples) override;
    void setCurrentPlaybackSampleRate(double newSampleRate) override;

    void setWaveform(Waveform newWaveform) noexcept;

    // setId identifies the immutable snapshot that owns `table`
    // (0 = none); see WavetableSnapshotStore.h.
    void setCustomWaveform(const BandLimitedWavetable* table, std::uint64_t setId) noexcept;

    // Smallest id of a custom wavetable snapshot this voice may still read
    // (its current table, or the one it is crossfading from), 0 if none.
    std::uint64_t getCustomSetIdInUse() const noexcept;

    void setADSRParameters(const juce::ADSR::Parameters& newParameters) noexcept;
    void setAmplitudeEnvelopeEnabled(bool enabled) noexcept;

    void setFilterEnvelopeParameters(const juce::ADSR::Parameters& parameters,
                                     float baseCutoff, float amountOctaves) noexcept;

    void setVoiceFilterEnabled(bool enabled) noexcept
    {
        if (enabled && !voiceLowPassEnabled)
            voiceLowPass.reset();
        voiceLowPassEnabled = enabled;
        voiceFilterMix.setTargetValue(enabled ? 1.0f : 0.0f);
    }

    // Oscillator bypass is smoothed even for notes already playing.
    void setOscillatorEnabled(bool enabled) noexcept
    {
        oscillatorGate.setTargetValue(enabled ? 1.0f : 0.0f);
    }

    void setUnisonEnabled(bool enabled) noexcept;

    // spread: 0 = mono, 1 = outermost oscillators hard left/right.
    void setUnisonParameters(int count, float detuneCents, float spread) noexcept;

private:
    static constexpr int maxUnisonOscillators = 7;

    // Pushes the current waveform/custom table to every oscillator.
    void applyTables() noexcept;
    // Recomputes count, detune frequencies, pans and oscillator gain targets.
    // `ramp` = glide to the new values (live change), else set them at once.
    void applyUnison(bool ramp) noexcept;
    void applyAmplitudeEnvelopeParameters() noexcept;
    int oscillatorsToRender() noexcept;

    // --- settings, as last received from the processor
    Waveform waveform = Waveform::Sawtooth;
    const BandLimitedWavetable* customTable = nullptr;
    std::uint64_t customSetId = 0;
    int unisonCount = 1;
    float detuneCents = 0.0f;
    float spread = 0.5f;
    bool unisonEnabled = true;
    bool amplitudeEnvelopeEnabled = true;
    juce::ADSR::Parameters amplitudeParameters {0.01f, 0.10f, 0.70f, 0.20f};
    juce::ADSR::Parameters filterParameters {0.01f, 0.35f, 0.0f, 0.20f};
    float voiceFilterBaseCutoff = 800.0f;
    float voiceFilterAmountOctaves = 3.0f;

    // --- state of the sounding note
    double noteFrequency = 440.0;
    int activeUnisonCount = 1;
    std::array<WavetableOscillator, maxUnisonOscillators> oscillators;
    std::array<bool, maxUnisonOscillators> rendering {};
    using Ramp = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    std::array<Ramp, maxUnisonOscillators> oscillatorGain, panLeft, panRight;
    Ramp unisonNormalisation {1.0f};

    // Custom snapshot ids: the one played now, and the one being faded out.
    std::uint64_t latchedCustomSetId = 0;
    std::uint64_t fadingCustomSetId = 0;
    int fadingCustomSamples = 0;
    int transitionSamples = 480;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> oscillatorGate {1.0f};

    LiveADSR envelope;
    LiveADSR filterEnvelope;

    juce::dsp::StateVariableTPTFilter<float> voiceLowPass;
    bool voiceLowPassEnabled = true;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> voiceFilterMix {1.0f};

    // Velocity gain, ramped when a stolen voice changes velocity.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> noteGain {0.0f};

    // Set by a hard stopNote() while still sounding. If startNote() follows
    // (voice stealing), the new note continues from the current level.
    bool retriggerFromCurrentLevel = false;
};

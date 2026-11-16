#include <cmath>
#include <utility>
#include "SynthVoice.h"
#include "SynthSound.h"
#include "WavetableOscillator.h"


bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound){
    return sound != nullptr;
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int pitchWheelPosition){
    
    juce::ignoreUnused(sound, pitchWheelPosition);

    // juce::Synthesiser steals a voice by calling stopNote(..., false) and
    // then startNote() straight away. Resetting envelopes and filter here
    // would drop the old note to zero in one sample (an audible click), so a
    // stolen voice keeps them: ADSR::noteOn() starts the attack from the
    // current level and the oscillator phases simply continue.
    const bool stolen = std::exchange(retriggerFromCurrentLevel, false);

    const double frequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    
    activeUnisonCount = pendingUnisonEnabled ? pendindUnisonCount : 1;
    // Detuned oscillators add up like uncorrelated signals (power), so the
    // sum is divided by sqrt(N), not N: 7 voices are as loud as 1, not -8.6 dB.
    unisonNormalisation = 1.0f / std::sqrt(static_cast<float>(activeUnisonCount));

    for (int i = 0; i < activeUnisonCount; ++i){

        const double position = activeUnisonCount == 1 ? 0.0 : 2.0 * static_cast<double>(i)/static_cast<double>(activeUnisonCount - 1) -1.0;
        const double cents = position * static_cast<double>(pendingDetuneCents);
        const double detuneFrequency = frequency * std::exp2(cents/1200.0);

        auto& oscillator = oscillators[static_cast<std::size_t>(i)];
        oscillator.setWaveform(pendingWaveform);
        oscillator.setCustomTable(pendingCustomTable);
        oscillator.setFrequency(detuneFrequency);

        // Stereo spread follows the detune position: the most detuned
        // oscillators sit furthest out. Equal-power pan law, scaled so a
        // centred oscillator keeps gain 1 on both channels (as in mono).
        const double angle = (1.0 + position * static_cast<double>(pendingSpread))
                             * juce::MathConstants<double>::pi / 4.0;
        panLeft[static_cast<std::size_t>(i)] = static_cast<float>(std::sqrt(2.0) * std::cos(angle));
        panRight[static_cast<std::size_t>(i)] = static_cast<float>(std::sqrt(2.0) * std::sin(angle));
    }

    // Only a voice that actually reads the custom table keeps its snapshot alive.
    latchedCustomSetId = pendingWaveform == Waveform::Custom && pendingCustomTable != nullptr
        ? pendingCustomSetId : 0;

    const float gain = velocity * voiceLevel;
    if (stolen)
    {
        noteGain.setTargetValue(gain);
    }
    else
    {
        voiceLowPass.reset();
        noteGain.setCurrentAndTargetValue(gain);
        envelope.reset();
        filterEnvelope.reset();
    }

    // Bypassing ADSR means a constant-level note with short anti-click
    // transitions. The note still ends correctly on MIDI Note Off.
    const juce::ADSR::Parameters gatedParameters {0.003f, 0.003f, 1.0f, 0.003f};
    envelope.setParameters(pendingAmplitudeEnvelopeEnabled
                               ? pendingADSRParameters : gatedParameters);
    envelope.noteOn();

    filterEnvelope.setParameters(filterASDRParameters);
    filterEnvelope.noteOn();
}

void SynthVoice::stopNote(float velocity, bool allowTailOff){

    juce::ignoreUnused(velocity);

    if (allowTailOff && envelope.isActive()){
        envelope.noteOff();
        filterEnvelope.noteOff();
    } else {
        // Hard stop: the voice goes silent now. If this is a steal,
        // startNote() follows immediately and continues from this level;
        // otherwise renderNextBlock() discards the stale state.
        retriggerFromCurrentLevel = envelope.isActive();
        clearCurrentNote();
    }
}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples){

    if (!isVoiceActive())
    {
        // A hard stop that was not a steal: start the next note from zero.
        if (std::exchange(retriggerFromCurrentLevel, false))
        {
            envelope.reset();
            filterEnvelope.reset();
            voiceLowPass.reset();
        }
        return;
    }

    const bool stereo = outputBuffer.getNumChannels() > 1;
    const float maxCutoff = juce::jmin(18000.0f, static_cast<float>(0.45 * getSampleRate()));

    for (int sample = 0; sample < numSamples; ++sample){

        const float envelopeValue = envelope.getNextSample();

        if (!envelope.isActive()){
            clearCurrentNote();
            break;
        }

        const float filterEnvValue = filterEnvelope.getNextSample();

        float left = 0.0f;
        float right = 0.0f;
        for (int osc = 0; osc < activeUnisonCount; ++osc){
            const auto index = static_cast<std::size_t>(osc);
            const float value = oscillators[index].getNextSample();
            left += value * panLeft[index];
            right += value * panRight[index];
        }
        left *= unisonNormalisation;
        right *= unisonNormalisation;

        const float filterWet = voiceFilterMix.getNextValue();

        if (filterWet > 0.0f){

            const float modulatedCutoff = voiceFilterBaseCutoff * std::exp2(voiceFilterAmountOctaves * filterEnvValue);
            voiceLowPass.setCutoffFrequency(juce::jlimit(20.0f, maxCutoff, modulatedCutoff));

            const float filteredLeft = voiceLowPass.processSample(0, left);
            const float filteredRight = voiceLowPass.processSample(1, right);
            left += filterWet * (filteredLeft - left);
            right += filterWet * (filteredRight - right);
        }

        const float gain = envelopeValue * noteGain.getNextValue()
                         * oscillatorGate.getNextValue();

        if (stereo){
            outputBuffer.addSample(0, startSample + sample, left * gain);
            outputBuffer.addSample(1, startSample + sample, right * gain);
        } else {
            outputBuffer.addSample(0, startSample + sample, 0.5f * (left + right) * gain);
        }
    }
}


void SynthVoice::setCurrentPlaybackSampleRate(
    double newSampleRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate(
        newSampleRate
    );

    if (newSampleRate <= 0.0)
        return;

    // Spread the start phases (golden-ratio steps): unison oscillators that
    // all start at phase 0 add up coherently into a loud first transient.
    for (std::size_t i = 0; i < oscillators.size(); ++i){
        oscillators[i].prepare(newSampleRate);
        oscillators[i].setPhase(0.6180339887 * static_cast<double>(i));
    }

    envelope.setSampleRate(newSampleRate);
    envelope.reset();
    envelope.setParameters(pendingADSRParameters);

    filterEnvelope.setSampleRate(newSampleRate);
    filterEnvelope.reset();
    filterEnvelope.setParameters(filterASDRParameters);

    juce::dsp::ProcessSpec spec {newSampleRate, 1u, 2u}; // stereo voice filter

    voiceLowPass.prepare(spec);
    voiceLowPass.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    voiceLowPass.setCutoffFrequency(800.0f);
    voiceLowPass.setResonance(0.7071f);
    voiceLowPass.reset();
    oscillatorGate.reset(newSampleRate, 0.005);
    oscillatorGate.setCurrentAndTargetValue(1.0f);
    voiceFilterMix.reset(newSampleRate, 0.005);
    voiceFilterMix.setCurrentAndTargetValue(voiceLowPassEnabled ? 1.0f : 0.0f);
    noteGain.reset(newSampleRate, 0.005);
    noteGain.setCurrentAndTargetValue(0.0f);
    retriggerFromCurrentLevel = false;
}

void SynthVoice::pitchWheelMoved(int newValue)
{
    juce::ignoreUnused(newValue);
}

void SynthVoice::controllerMoved(
    int controllerNumber,
    int newValue)
{
    juce::ignoreUnused(controllerNumber, newValue);
}
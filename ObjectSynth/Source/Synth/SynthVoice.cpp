#include <cmath>
#include "SynthVoice.h"
#include "SynthSound.h"
#include "WavetableOscillator.h"


bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound){
    return sound != nullptr;
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int pitchWheelPosition){
    
    juce::ignoreUnused(sound, pitchWheelPosition);

    const double frequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    
    oscillator.setFrequency(frequency);
    voiceLowPass.reset();

    noteGain = velocity * 0.1f;
    
    envelope.reset();
    envelope.setParameters(pendingADSRParameters);
    envelope.noteOn();

    filterEnvelope.reset();
    filterEnvelope.setParameters(filterASDRParameters);
    filterEnvelope.noteOn();
}

void SynthVoice::stopNote(float velocity, bool allowTailOff){

    juce::ignoreUnused(velocity);

    if (allowTailOff && envelope.isActive()){
        envelope.noteOff();
        filterEnvelope.noteOff();
    } else {
        envelope.reset();
        filterEnvelope.reset();
        noteGain = 0.0f;
        clearCurrentNote();
    }
}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples){

    if (!isVoiceActive()) return;

    for (int i = 0; i < numSamples; ++i){

        const float envelopeValue = envelope.getNextSample();
        
        if (!envelope.isActive()){
            clearCurrentNote();
            break;
        }

        const float filterEnvValue = filterEnvelope.getNextSample();

        const float oscillatorValue = oscillator.getNextSample();

        float filteredValue = oscillatorValue;
        const float filterWet = voiceFilterMix.getNextValue();

        if (filterWet > 0.0f){

            const float modulatedCutoff = voiceFilterBaseCutoff * std::exp2(voiceFilterAmountOctaves * filterEnvValue);
            const float maxCutoff = juce::jmin(18000.0f, static_cast<float>(0.45 * getSampleRate()));
            const float safeCutoff = juce::jlimit(20.0f, maxCutoff, modulatedCutoff);

            voiceLowPass.setCutoffFrequency(safeCutoff);
            const float processed = voiceLowPass.processSample(0, oscillatorValue);
            filteredValue = filterWet >= 1.0f ? processed
                : oscillatorValue + filterWet * (processed - oscillatorValue);
        }

        const float value = filteredValue * envelopeValue * noteGain;

        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel){
            outputBuffer.addSample(channel, startSample + i, value);
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

    oscillator.prepare(newSampleRate);

    envelope.setSampleRate(newSampleRate);
    envelope.reset();
    envelope.setParameters(pendingADSRParameters);

    filterEnvelope.setSampleRate(newSampleRate);
    filterEnvelope.reset();
    filterEnvelope.setParameters(filterASDRParameters);

    juce::dsp::ProcessSpec spec {newSampleRate, 1u, 1u};

    voiceLowPass.prepare(spec);
    voiceLowPass.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    voiceLowPass.setCutoffFrequency(800.0f);
    voiceLowPass.setResonance(0.7071f);
    voiceLowPass.reset();
    voiceFilterMix.reset(newSampleRate, 0.005);
    voiceFilterMix.setCurrentAndTargetValue(voiceLowPassEnabled ? 1.0f : 0.0f);
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
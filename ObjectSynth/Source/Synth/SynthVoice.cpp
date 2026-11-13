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

    noteGain = velocity * 0.1f;
    
    envelope.reset();
    envelope.setParameters(pendingADSRParameters);
    envelope.noteOn();
}

void SynthVoice::stopNote(float velocity, bool allowTailOff){

    juce::ignoreUnused(velocity);

    if (allowTailOff && envelope.isActive()){
        envelope.noteOff();
    } else {
        envelope.reset();
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

        const float oscillatorValue = oscillator.getNextSample();
        const float value = oscillatorValue * envelopeValue * noteGain;

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
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "atomic"
#include "Synth/SynthSound.h"
#include "Synth/WavetableOscillator.h"
#include "DSP/FilterBank.h"

// Audio engine: receives audio buffers and MIDI events.
// The first wavetable oscillator will be connected here in the next milestone.
class ObjectSynthAudioProcessor final : public juce::AudioProcessor
{
public:
    ObjectSynthAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midiMessages) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    juce::MidiKeyboardState& getKeyboardState() noexcept {
        return keyboardState;
    }

    void setWaveform(Waveform newWaveform) noexcept {
        requestedWaveform.store(newWaveform, std::memory_order_relaxed);
    }

    Waveform getWaveform() const noexcept {
         return requestedWaveform.load(std::memory_order_relaxed);
    }

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept{
        return apvts;
    }

private:

    juce::Synthesiser synthesiser;

    std::atomic<Waveform> requestedWaveform {Waveform::Sawtooth};
    Waveform appliedWaveform = Waveform::Sawtooth;

    juce::MidiKeyboardState keyboardState;

    juce::dsp::WaveShaper<float> distortion;
    juce::dsp::Gain<float> distortionDrive;
    juce::dsp::DryWetMixer<float> distortionMixer;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* attackParameter = nullptr;
    std::atomic<float>* decayParameter = nullptr;
    std::atomic<float>* sustainParameter = nullptr;
    std::atomic<float>* releaseParameter = nullptr;

    std::atomic<float>* distortioDriveParameter = nullptr;
    std::atomic<float>* distortioMixParameter = nullptr;

    std::atomic<float>* chorusRateParameter = nullptr;
    std::atomic<float>* chorusDepthParameter = nullptr;
    std::atomic<float>* chorusDelayParameter = nullptr;
    std::atomic<float>* chorusMixParameter = nullptr;
    
    FilterBank filterBank;
    std::atomic<float>*  lpCutoffParameter = nullptr;
    std::atomic<float>*  lpResonanceParameter = nullptr;
    std::atomic<float>*  hpCutoffParameter = nullptr;
    std::atomic<float>*  hpResonanceParameter = nullptr;
    std::atomic<float>*  bpCutoffParameter = nullptr;
    std::atomic<float>*  bpResonanceParameter = nullptr;
    std::atomic<float>*  lpMixParameter = nullptr;
    std::atomic<float>*  hpMixParameter = nullptr;
    std::atomic<float>*  bpMixParameter = nullptr;

    juce::dsp::Chorus<float> chorus;    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ObjectSynthAudioProcessor)
};

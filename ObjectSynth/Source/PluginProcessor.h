#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

#include "Synth/SynthSound.h"
#include "Synth/WavetableOscillator.h"
#include "DSP/FilterBank.h"
#include "DSP/LFO.h"
#include "DSP/ControlRateScheduler.h"
#include "DSP/ModulationSmoother.h"
#include "DSP/ModulationMatrix.h"

class ObjectSynthAudioProcessor final : public juce::AudioProcessor
{
public:
    ObjectSynthAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
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
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    void setWaveform(Waveform newWaveform) noexcept
    {
        requestedWaveform.store(newWaveform, std::memory_order_relaxed);
    }

    Waveform getWaveform() const noexcept
    {
        return requestedWaveform.load(std::memory_order_relaxed);
    }

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    // GUI reads a single atomic sample; it never touches the audio LFO state.
    float getLfoPhaseForUI() const noexcept
    {
        return lfoPhaseForUI.load(std::memory_order_relaxed);
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct ModulationMapping
    {
        float base = 0.0f;
        float minimum = 0.0f;
        float maximum = 1.0f;
        float excursion = 0.0f;
        bool logarithmic = false;
    };

    ModulationMapping getMapping(ModDestination destination, float maxCutoff) const noexcept;

    juce::Synthesiser synthesiser;
    std::atomic<Waveform> requestedWaveform { Waveform::Sawtooth };
    Waveform appliedWaveform = Waveform::Sawtooth;
    juce::MidiKeyboardState keyboardState;

    juce::dsp::WaveShaper<float> distortion;
    juce::dsp::Gain<float> distortionDrive;
    juce::dsp::DryWetMixer<float> distortionMixer;
    juce::dsp::Chorus<float> chorus;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float>* attackParameter = nullptr;
    std::atomic<float>* decayParameter = nullptr;
    std::atomic<float>* sustainParameter = nullptr;
    std::atomic<float>* releaseParameter = nullptr;

    // Preserve original IDs (including these C++ member names) for compatibility.
    std::atomic<float>* distortioDriveParameter = nullptr;
    std::atomic<float>* distortioMixParameter = nullptr;

    std::atomic<float>* chorusRateParameter = nullptr;
    std::atomic<float>* chorusDepthParameter = nullptr;
    std::atomic<float>* chorusDelayParameter = nullptr;
    std::atomic<float>* chorusMixParameter = nullptr;

    std::atomic<float>* filterEnvAttackParameter = nullptr;
    std::atomic<float>* filterEnvDecayParameter = nullptr;
    std::atomic<float>* filterEnvSustainParameter = nullptr;
    std::atomic<float>* filterEnvReleaseParameter = nullptr;
    std::atomic<float>* voiceFilterCutoffParameter = nullptr;
    std::atomic<float>* filterEnvAmountParameter = nullptr;

    std::atomic<float>* lpCutoffParameter = nullptr;
    std::atomic<float>* lpResonanceParameter = nullptr;
    std::atomic<float>* hpCutoffParameter = nullptr;
    std::atomic<float>* hpResonanceParameter = nullptr;
    std::atomic<float>* bpCutoffParameter = nullptr;
    std::atomic<float>* bpResonanceParameter = nullptr;
    std::atomic<float>* lpMixParameter = nullptr;
    std::atomic<float>* hpMixParameter = nullptr;
    std::atomic<float>* bpMixParameter = nullptr;

    std::atomic<float>* lfoEnabledParameter = nullptr;
    std::atomic<float>* lfoRateParameter = nullptr;
    std::atomic<float>* lfoWaveformParameter = nullptr;
    std::array<std::atomic<float>*, ModulationMatrix::maxRoutes> routeDestinationParameters {};
    std::array<std::atomic<float>*, ModulationMatrix::maxRoutes> routeDepthParameters {};

    std::atomic<float>* filterBankEnabledParameter = nullptr;
    std::atomic<float>* filterEnvEnabledParameter = nullptr;
    std::atomic<float>* distortionEnabledParameter = nullptr;
    std::atomic<float>* chorusEnabledParameter = nullptr;

    LFO globalLFO;
    ControlRateScheduler lfoScheduler;
    ModulationMatrix modulationMatrix;
    std::array<ModulationSmoother, ModulationMatrix::destinationCount> destinationSmoothers {};
    ModulationSmoother filterBankBypassSmoother;
    ModulationSmoother distortionBypassSmoother;
    ModulationSmoother chorusBypassSmoother;
    std::atomic<float> lfoPhaseForUI {0.0f};

    FilterBank filterBank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ObjectSynthAudioProcessor)
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <utility>
#include <atomic>
#include <mutex>
#include <memory>
#include <vector>

#include "Synth/SynthSound.h"
#include "Synth/WavetableOscillator.h"
#include "Synth/WavetableBank.h"
#include "Synth/WavetableLoader.h"
#include "Synth/WavetableSnapshotStore.h"
#include "DSP/FilterBank.h"
#include "DSP/Saturator.h"
#include "DSP/StereoChorus.h"
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

    void setWaveform(Waveform newWaveform) noexcept {
        requestedWaveform.store(newWaveform, std::memory_order_relaxed);
    }

    Waveform getWaveform() const noexcept {
        return requestedWaveform.load(std::memory_order_relaxed);
    }

    bool loadCustomWav(const juce::File& file, juce::String& message);
    bool selectCustomFrame(int frameIndex);
    int getCustomFrameIndex() const noexcept
    {
        return currentCustomFrame.load(std::memory_order_acquire);
    }
    int getCustomFrameCount() const noexcept
    {
        const auto* set = currentCustomSet.load(std::memory_order_acquire);
        return set != nullptr ? static_cast<int>(set->frames.size()) : 0;
    }

    // For GUI display only. Immutable snapshots are retained for the life of the processor.
    const WavetableBank::Table* getCustomWavetableForUI() const noexcept
    {
        return currentCustomTable.load(std::memory_order_acquire);
    }
    bool hasCustomWavetable() const noexcept
    {
        return getCustomWavetableForUI() != nullptr;
    }
    bool getCustomSeamWarning() const noexcept
    {
        return customSeamWarning.load(std::memory_order_acquire);
    }
    juce::String getCustomWavetableName() const;

    // Diagnostics/tests: custom wavetable snapshots currently kept in memory.
    std::size_t getRetainedWavetableCount() const;

    // Portable user preset files. Never call these from the audio thread.
    bool savePresetFile(const juce::File& file, juce::String& message);
    bool loadPresetFile(const juce::File& file, juce::String& message);

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    // GUI reads a single atomic sample; it never touches the audio LFO state.
    float getLfoPhaseForUI() const noexcept
    {
        return lfoPhaseForUI.load(std::memory_order_relaxed);
    }

    // Output meter (GUI thread): peak |sample| of a channel (0 = L, 1 = R)
    // since the previous call, after the Volume stage. Lock-free.
    float consumeOutputPeak(int channel) noexcept
    {
        return outputPeak[static_cast<std::size_t>(juce::jlimit(0, 1, channel))]
            .exchange(0.0f, std::memory_order_acq_rel);
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

    using CustomTable = WavetableBank::Table;
    struct CustomWavetableSet
    {
        std::uint64_t id = 0; // assigned by SnapshotStore::publish()
        std::vector<CustomTable> frames;
        std::vector<bool> seamWarnings;
    };

    // Snapshots are immutable once published. An old snapshot is freed (on
    // the message thread) only after the audio thread reports that no
    // sounding voice can still read it; see WavetableSnapshotStore.h.
    std::atomic<const CustomWavetableSet*> currentCustomSet {nullptr};
    std::atomic<const CustomTable*> currentCustomTable {nullptr}; // GUI preview
    std::atomic<int> currentCustomFrame {0};
    SnapshotStore<CustomWavetableSet> customTableStore;
    mutable std::mutex customTableMutex;
    juce::String customWavetableName;
    std::atomic<bool> customSeamWarning {false};
    void publishCustomFrames(std::vector<CustomTable> frames,
                             std::vector<bool> seamWarnings,
                             const juce::String& name,
                             int selectedFrame = 0);
    // Decodes the preset's Base64 wavetable blob (format v1 or v2).
    static bool parseEmbeddedWavetable(const juce::String& encoded,
                                       bool legacySeamWarning,
                                       std::vector<CustomTable>& frames,
                                       std::vector<bool>& warnings);

    juce::Synthesiser synthesiser;
    // MIDI events of the current 32-sample chunk only. juce::Synthesiser
    // handles every event at/after the chunk start, so it must never see
    // the whole host buffer while rendering a chunk. Preallocated.
    juce::MidiBuffer chunkMidi;
    std::atomic<Waveform> requestedWaveform { Waveform::Sawtooth };
    Waveform appliedWaveform = Waveform::Sawtooth;
    juce::MidiKeyboardState keyboardState;

    Saturator distortionStage;      // oversampled, loudness-compensated
    juce::dsp::Gain<float> outputGain; // Volume parameter, ramped
    std::array<std::atomic<float>, 2> outputPeak {}; // max |sample| since last GUI read
    StereoChorus chorus;            // right LFO 90 degrees ahead: real stereo
    float panGainLeft = 1.0f, panGainRight = 1.0f; // last applied balance gains
    float modulatedPan = 0.0f; // updated at control rate, used by every chunk

    // Balance law: the side away from the pan direction is attenuated with
    // a quarter cosine; the other side stays at unity (never boosted).
    static std::pair<float, float> balanceGains(float pan) noexcept
    {
        const float p = juce::jlimit(-1.0f, 1.0f, pan);
        const float halfPi = juce::MathConstants<float>::halfPi;
        return {p > 0.0f ? std::cos(p * halfPi) : 1.0f,
                p < 0.0f ? std::cos(-p * halfPi) : 1.0f};
    }

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

    std::atomic<float>* unisonCountParameter = nullptr;
    std::atomic<float>* unisonDetuneParameter = nullptr;
    std::atomic<float>* unisonSpreadParameter = nullptr;
    std::atomic<float>* outputPanParameter = nullptr;

    std::atomic<float>* oscillatorEnabledParameter = nullptr;
    std::atomic<float>* amplitudeEnvelopeEnabledParameter = nullptr;
    std::atomic<float>* unisonEnabledParameter = nullptr;
    std::atomic<float>* lfoEnabledParameter = nullptr;
    std::atomic<float>* lfoRateParameter = nullptr;
    std::atomic<float>* lfoWaveformParameter = nullptr;
    std::array<std::atomic<float>*, ModulationMatrix::maxRoutes> routeDestinationParameters {};
    std::array<std::atomic<float>*, ModulationMatrix::maxRoutes> routeDepthParameters {};

    std::atomic<float>* filterBankEnabledParameter = nullptr;
    std::atomic<float>* filterEnvEnabledParameter = nullptr;
    std::atomic<float>* distortionEnabledParameter = nullptr;
    std::atomic<float>* chorusEnabledParameter = nullptr;
    std::atomic<float>* outputVolumeParameter = nullptr;

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

#include <cmath>
#include <algorithm>
#include <limits>
#include <utility>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Synth/SynthVoice.h"
#include "Synth/SynthSound.h"

juce::AudioProcessorValueTreeState::ParameterLayout 
ObjectSynthAudioProcessor::createParameterLayout(){

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"attack", 1}, "Attack", juce::NormalisableRange<float>(0.0f, 5.0f, 0.001f), 0.005f
    ));
    
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"decay", 1}, "Decay", juce::NormalisableRange<float>(0.0f, 5.0f, 0.001f), 0.1f
    ));
    
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"sustain", 1}, "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.7f
    ));
    
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"release", 1}, "Release", juce::NormalisableRange<float>(0.0f, 5.0f, 0.001f), 0.1f
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lpCutoff",1}, "LP Cutoff", juce::NormalisableRange<float>(20.0f, 18000.0f, 1.0f, 0.3f), 18000.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"hpCutoff", 1}, "HP Cutoff", juce::NormalisableRange<float>{20.0f, 18000.0f, 1.0f, 0.3f}, 180.0f
    ));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"bpCutoff", 1}, "BP Cutoff", juce::NormalisableRange<float>{20.0f, 18000.0f, 1.0f, 0.3f},1200.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lpResonance", 1}, "LP Resonance",juce::NormalisableRange<float>{0.3f, 5.0f, 0.01f}, 0.7071f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"hpResonance", 1}, "HP Resonance", juce::NormalisableRange<float>{0.3f, 5.0f, 0.01f}, 0.71f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"bpResonance", 1}, "BP Resonance", juce::NormalisableRange<float>{0.3f, 5.0f, 0.01f}, 1.2f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lpMix",1}, "Low-Pass Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f
    ));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"hpMix",1}, "High-Pass Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f
    ));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"bpMix",1}, "Band-Pass Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"distortionDrive", 1}, "Distortion Drive", juce::NormalisableRange<float>{1.0f, 10.0f, 0.01f}, 3.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"distortionMix", 1}, "Distortion Mix", juce::NormalisableRange<float>{0.0f, 1.0f, 0.01f}, 1.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"chorusRate", 1}, "Chorus Rate", juce::NormalisableRange<float>{0.05f, 5.0f, 0.01f}, 0.8f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"chorusDepth", 1}, "Chorus Depth", juce::NormalisableRange<float>{0.0f, 1.0f, 0.01f}, 0.3f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"chorusDelay", 1}, "Chorus Centre Delay", juce::NormalisableRange<float>{1.0f, 30.0f, 0.1f}, 7.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"chorusMix", 1}, "Chorus Mix", juce::NormalisableRange<float>{0.0f, 1.0f, 0.01f}, 0.4f
    ));


    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"filterEnvAttack", 1}, "Filter Env Attack", juce::NormalisableRange<float>{0.0f, 5.0f, 0.001f}, 0.01f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"filterEnvDecay", 1}, "Filter Env Decay", juce::NormalisableRange<float>{0.0f, 5.0f, 0.001f}, 0.35f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"filterEnvSustain", 1}, "Filter Env Sustain", juce::NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 0.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"filterEnvRelease", 1}, "Filter Env Release", juce::NormalisableRange<float>{0.0f, 5.0f, 0.001f}, 0.20f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"voiceFilterCutoff", 1}, "Voice Filter Cutoff", juce::NormalisableRange<float>{20.0f, 18000.0f, 1.0f, 0.3f}, 800.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"filterEnvAmount", 1}, "Filter Env Amount", juce::NormalisableRange<float>{-4.0f, 4.0f, 0.1f}, 0.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"unisonCount", 1}, "Unison Oscillators", 1, 7 , 1
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"unisonDetune", 1}, "Unison Detune", 0.0f, 25.0f, 0.0f
    ));

    // 0 = all unison oscillators centred (mono), 1 = outermost hard L/R.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"unisonSpread", 1}, "Unison Spread",
        juce::NormalisableRange<float>{0.0f, 1.0f, 0.01f}, 0.5f));

    // Module bypass is deliberately independent from Dry/Wet and Mix.
    // Neutral init: only the oscillator and amplitude envelope start enabled.
    for (const auto* id : {"oscillatorEnabled", "amplitudeEnvelopeEnabled"})
    {
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID {id, 1}, juce::String(id), true));
    }

    for (const auto* id : {"unisonEnabled", "filterBankEnabled", "filterEnvEnabled",
                           "distortionEnabled", "chorusEnabled", "lfoEnabled"})
    {
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID {id, 1}, juce::String(id), false));
    }

    // LFO rate is tempo-synced to the host (note divisions, see DSP/TempoSync.h).
    juce::StringArray syncDivisions;
    for (const auto& division : TempoSync::divisions)
        syncDivisions.add(division.name);

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"lfoSync", 1}, "LFO Rate", syncDivisions,
        TempoSync::defaultIndex));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"lfoWaveform", 1}, "LFO Waveform",
        juce::StringArray {"Sine", "Saw", "Square", "Triangle", "Custom"}, 0));

    // Replaces the old fixed 0.5 master attenuation.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID {"outputVolume", 1}, "Output Volume",
        juce::NormalisableRange<float> {-48.0f, 6.0f, 0.1f, 2.0f}, -6.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    // Output balance: -1 = left only, 0 = centre, +1 = right only.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID {"outputPan", 1}, "Output Pan",
        juce::NormalisableRange<float> {-1.0f, 1.0f, 0.01f}, 0.0f));

    juce::StringArray destinations;
    for (const char* name : modDestinationNames)
        destinations.add(name);

    for (std::size_t i = 0; i < ModulationMatrix::maxRoutes; ++i)
    {
        const auto number = juce::String(static_cast<int>(i + 1));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID {juce::String("lfoRoute") + number + "Destination", 1},
            juce::String("LFO Route ") + number + " Destination", destinations,
            i == 0 ? static_cast<int>(ModDestination::LPCutoff) : 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID {juce::String("lfoRoute") + number + "Depth", 1},
            juce::String("LFO Route ") + number + " Depth",
            juce::NormalisableRange<float> {-1.0f, 1.0f, 0.01f},
            i == 0 ? 0.5f : 0.0f));
    }

    return layout;
}


ObjectSynthAudioProcessor::ObjectSynthAudioProcessor(): 
AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())       
{

    attackParameter = apvts.getRawParameterValue("attack");
    decayParameter = apvts.getRawParameterValue("decay");
    sustainParameter = apvts.getRawParameterValue("sustain");
    releaseParameter = apvts.getRawParameterValue("release");
    jassert(attackParameter != nullptr);
    jassert(decayParameter != nullptr);
    jassert(sustainParameter != nullptr);
    jassert(releaseParameter != nullptr);

    distortioDriveParameter = apvts.getRawParameterValue("distortionDrive");
    distortioMixParameter = apvts.getRawParameterValue("distortionMix");
    jassert(distortioDriveParameter != nullptr);
    jassert(distortioMixParameter != nullptr);


    chorusRateParameter = apvts.getRawParameterValue("chorusRate");
    chorusDepthParameter = apvts.getRawParameterValue("chorusDepth");
    chorusDelayParameter = apvts.getRawParameterValue("chorusDelay");
    chorusMixParameter = apvts.getRawParameterValue("chorusMix");

    jassert(chorusRateParameter != nullptr);
    jassert(chorusDepthParameter != nullptr);
    jassert(chorusDelayParameter != nullptr);
    jassert(chorusMixParameter != nullptr);

    filterEnvAttackParameter = apvts.getRawParameterValue("filterEnvAttack");
    filterEnvDecayParameter = apvts.getRawParameterValue("filterEnvDecay");
    filterEnvSustainParameter = apvts.getRawParameterValue("filterEnvSustain");
    filterEnvReleaseParameter = apvts.getRawParameterValue("filterEnvRelease");
    voiceFilterCutoffParameter = apvts.getRawParameterValue("voiceFilterCutoff");
    filterEnvAmountParameter = apvts.getRawParameterValue("filterEnvAmount");

    jassert(filterEnvAttackParameter != nullptr);
    jassert(filterEnvDecayParameter != nullptr);
    jassert(filterEnvSustainParameter != nullptr);
    jassert(filterEnvReleaseParameter != nullptr);
    jassert(voiceFilterCutoffParameter != nullptr);
    jassert(filterEnvAmountParameter != nullptr); 

    unisonCountParameter = apvts.getRawParameterValue("unisonCount");
    unisonDetuneParameter = apvts.getRawParameterValue("unisonDetune");
    jassert(unisonCountParameter != nullptr);
    jassert(unisonDetuneParameter != nullptr);
    unisonSpreadParameter = apvts.getRawParameterValue("unisonSpread");
    outputPanParameter = apvts.getRawParameterValue("outputPan");
    jassert(unisonSpreadParameter != nullptr && outputPanParameter != nullptr);
    
    lpCutoffParameter = apvts.getRawParameterValue("lpCutoff");
    lpResonanceParameter = apvts.getRawParameterValue("lpResonance");
    hpCutoffParameter = apvts.getRawParameterValue("hpCutoff");
    hpResonanceParameter = apvts.getRawParameterValue("hpResonance");
    bpCutoffParameter = apvts.getRawParameterValue("bpCutoff");
    bpResonanceParameter = apvts.getRawParameterValue("bpResonance");
    lpMixParameter = apvts.getRawParameterValue("lpMix");
    hpMixParameter = apvts.getRawParameterValue("hpMix");
    bpMixParameter = apvts.getRawParameterValue("bpMix");
    jassert(lpCutoffParameter != nullptr);
    jassert(lpResonanceParameter != nullptr);
    jassert(hpCutoffParameter != nullptr);
    jassert(hpResonanceParameter != nullptr);
    jassert(bpCutoffParameter != nullptr);
    jassert(bpResonanceParameter != nullptr);
    jassert(lpMixParameter != nullptr);
    jassert(hpMixParameter != nullptr);
    jassert(bpMixParameter != nullptr);
    
    oscillatorEnabledParameter = apvts.getRawParameterValue("oscillatorEnabled");
    amplitudeEnvelopeEnabledParameter = apvts.getRawParameterValue("amplitudeEnvelopeEnabled");
    unisonEnabledParameter = apvts.getRawParameterValue("unisonEnabled");
    jassert(oscillatorEnabledParameter && amplitudeEnvelopeEnabledParameter && unisonEnabledParameter);

    lfoEnabledParameter = apvts.getRawParameterValue("lfoEnabled");
    lfoSyncParameter = apvts.getRawParameterValue("lfoSync");
    lfoWaveformParameter = apvts.getRawParameterValue("lfoWaveform");
    filterBankEnabledParameter = apvts.getRawParameterValue("filterBankEnabled");
    filterEnvEnabledParameter = apvts.getRawParameterValue("filterEnvEnabled");
    distortionEnabledParameter = apvts.getRawParameterValue("distortionEnabled");
    chorusEnabledParameter = apvts.getRawParameterValue("chorusEnabled");

    jassert(lfoEnabledParameter && lfoSyncParameter && lfoWaveformParameter);
    jassert(filterBankEnabledParameter && filterEnvEnabledParameter);
    jassert(distortionEnabledParameter && chorusEnabledParameter);

    for (std::size_t i = 0; i < ModulationMatrix::maxRoutes; ++i)
    {
        const auto number = juce::String(static_cast<int>(i + 1));
        routeDestinationParameters[i] = apvts.getRawParameterValue(
            juce::String("lfoRoute") + number + "Destination");
        routeDepthParameters[i] = apvts.getRawParameterValue(
            juce::String("lfoRoute") + number + "Depth");
        jassert(routeDestinationParameters[i] && routeDepthParameters[i]);
    }

    outputVolumeParameter = apvts.getRawParameterValue("outputVolume");
    jassert(outputVolumeParameter != nullptr);

    for (auto& value : modulatedForUI)
        value.store(std::numeric_limits<float>::quiet_NaN(), std::memory_order_relaxed);

    for (int i = 0; i < 8; ++i){
        synthesiser.addVoice(new SynthVoice());
    }

    synthesiser.addSound(new SynthSound());

}


ObjectSynthAudioProcessor::ModulationMapping
ObjectSynthAudioProcessor::getMapping(ModDestination destination, float maxCutoff) const noexcept
{
    const auto value = [](const std::atomic<float>* p) noexcept
    {
        return p->load(std::memory_order_relaxed);
    };

    switch (destination)
    {
        // Cutoff modulation is musical: up to +/- four octaves.
        case ModDestination::LPCutoff: return {value(lpCutoffParameter), 20.0f, maxCutoff, 4.0f, true};
        case ModDestination::HPCutoff: return {value(hpCutoffParameter), 20.0f, maxCutoff, 4.0f, true};
        case ModDestination::BPCutoff: return {value(bpCutoffParameter), 20.0f, maxCutoff, 4.0f, true};
        case ModDestination::VoiceFilterCutoff: return {value(voiceFilterCutoffParameter), 20.0f, maxCutoff, 4.0f, true};

        case ModDestination::LPResonance: return {value(lpResonanceParameter), 0.3f, 5.0f, 2.35f, false};
        case ModDestination::HPResonance: return {value(hpResonanceParameter), 0.3f, 5.0f, 2.35f, false};
        case ModDestination::BPResonance: return {value(bpResonanceParameter), 0.3f, 5.0f, 2.35f, false};
        case ModDestination::LPMix: return {value(lpMixParameter), 0.0f, 1.0f, 0.5f, false};
        case ModDestination::HPMix: return {value(hpMixParameter), 0.0f, 1.0f, 0.5f, false};
        case ModDestination::BPMix: return {value(bpMixParameter), 0.0f, 1.0f, 0.5f, false};

        case ModDestination::FilterEnvelopeAmount: return {value(filterEnvAmountParameter), -4.0f, 4.0f, 4.0f, false};
        case ModDestination::DistortionDrive: return {value(distortioDriveParameter), 1.0f, 10.0f, 4.5f, false};
        case ModDestination::DistortionMix: return {value(distortioMixParameter), 0.0f, 1.0f, 0.5f, false};
        case ModDestination::ChorusRate: return {value(chorusRateParameter), 0.05f, 5.0f, 2.475f, false};
        case ModDestination::ChorusDepth: return {value(chorusDepthParameter), 0.0f, 1.0f, 0.5f, false};
        case ModDestination::ChorusDelay: return {value(chorusDelayParameter), 1.0f, 30.0f, 14.5f, false};
        case ModDestination::ChorusMix: return {value(chorusMixParameter), 0.0f, 1.0f, 0.5f, false};
        // Depth 1 with the LFO = full auto-pan sweep.
        case ModDestination::OutputPan: return {value(outputPanParameter), -1.0f, 1.0f, 1.0f, false};

        case ModDestination::None:
        case ModDestination::Count: return {};
    }
    return {};
}

void ObjectSynthAudioProcessor::publishCustomFrames(
    std::vector<CustomTable> frames,
    std::vector<bool> seamWarnings,
    const juce::String& name,
    int selectedFrame)
{
    if (frames.empty() || frames.size() > 32)
        return;

    auto snapshot = std::make_unique<CustomWavetableSet>();
    snapshot->frames = std::move(frames);
    snapshot->seamWarnings = std::move(seamWarnings);
    if (snapshot->seamWarnings.size() != snapshot->frames.size())
        snapshot->seamWarnings.assign(snapshot->frames.size(), false);

    // Anti-aliased playback copies (FFT work), done here on the loading
    // thread and before taking the lock; the audio thread only reads them.
    snapshot->bandLimited.resize(snapshot->frames.size());
    for (std::size_t i = 0; i < snapshot->frames.size(); ++i)
        snapshot->bandLimited[i].build(snapshot->frames[i]);

    const int index = juce::jlimit(
        0, static_cast<int>(snapshot->frames.size()) - 1, selectedFrame);

    // This mutex is never used by the audio thread. publish() also frees
    // the snapshots that the audio thread reported as no longer in use.
    std::lock_guard<std::mutex> lock(customTableMutex);
    const auto* publishedSet = customTableStore.publish(std::move(snapshot));
    customWavetableName = name;
    currentCustomSet.store(publishedSet, std::memory_order_release);
    currentCustomFrame.store(index, std::memory_order_release);
    customSeamWarning.store(publishedSet->seamWarnings[static_cast<std::size_t>(index)],
                            std::memory_order_release);
    currentCustomTable.store(&publishedSet->frames[static_cast<std::size_t>(index)],
                             std::memory_order_release);
}

bool ObjectSynthAudioProcessor::selectCustomFrame(int frameIndex)
{
    // Called from the editor/message thread, never from processBlock().
    std::lock_guard<std::mutex> lock(customTableMutex);
    const auto* set = currentCustomSet.load(std::memory_order_acquire);
    if (set == nullptr || frameIndex < 0 ||
        frameIndex >= static_cast<int>(set->frames.size()))
        return false;

    const auto index = static_cast<std::size_t>(frameIndex);
    currentCustomFrame.store(frameIndex, std::memory_order_release);
    customSeamWarning.store(set->seamWarnings[index], std::memory_order_release);
    currentCustomTable.store(&set->frames[index], std::memory_order_release);
    return true;
}

bool ObjectSynthAudioProcessor::loadCustomWav(
    const juce::File& file, juce::String& message)
{
    auto result = WavetableLoader::loadSingleCycleWav(file);
    if (!result.success)
    {
        message = result.error;
        return false;
    }

    const auto count = result.frames.size();
    const bool firstWarning = result.seamWarning;
    publishCustomFrames(std::move(result.frames),
                        std::move(result.seamWarnings), file.getFileName());
    setObjectPhoto({}, {});
    setWaveform(Waveform::Custom);
    message = "Wavetable loaded: " + file.getFileName() +
        " (" + juce::String(static_cast<int>(count)) + " frame(s))";
    if (firstWarning)
        message += " - SEAM WARNING on frame 1";
    return true;
}

juce::MemoryBlock ObjectSynthAudioProcessor::getObjectPhoto(juce::String& name) const
{
    std::lock_guard<std::mutex> lock(objectPhotoMutex);
    name = objectName;
    return objectPhoto;
}

void ObjectSynthAudioProcessor::setObjectPhoto(juce::MemoryBlock jpeg, const juce::String& name)
{
    {
        std::lock_guard<std::mutex> lock(objectPhotoMutex);
        if (jpeg == objectPhoto && name == objectName)
            return;
        objectPhoto = std::move(jpeg);
        objectName = name;
    }
    objectPhotoVersion.fetch_add(1, std::memory_order_acq_rel);
}

juce::String ObjectSynthAudioProcessor::getCustomWavetableName() const
{
    std::lock_guard<std::mutex> lock(customTableMutex);
    return customWavetableName;
}

std::size_t ObjectSynthAudioProcessor::getRetainedWavetableCount() const
{
    std::lock_guard<std::mutex> lock(customTableMutex);
    return customTableStore.size();
}

bool ObjectSynthAudioProcessor::savePresetFile(
    const juce::File& file, juce::String& message)
{
    juce::MemoryBlock data;
    getStateInformation(data);
    if (data.isEmpty() || !file.replaceWithData(data.getData(), data.getSize()))
    {
        message = "Could not write preset file";
        return false;
    }
    message = "Preset saved";
    return true;
}

bool ObjectSynthAudioProcessor::loadPresetFile(
    const juce::File& file, juce::String& message)
{
    juce::MemoryBlock data;
    if (!file.existsAsFile() || file.getSize() > 2 * 1024 * 1024 ||
        !file.loadFileAsData(data) || data.isEmpty())
    {
        message = "Cannot read preset file";
        return false;
    }
    if (data.getSize() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        message = "Preset file is too large";
        return false;
    }
    // Validate before mutating the running synth.
    auto xml = getXmlFromBinary(data.getData(), static_cast<int>(data.getSize()));
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType()))
    {
        message = "Not a valid ObjectSynth preset";
        return false;
    }
    setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    message = "Preset loaded";
    return true;
}

void ObjectSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(juce::jmax(1, samplesPerBlock)),
        static_cast<juce::uint32>(getTotalNumOutputChannels())
    };

    filterBank.prepare(spec);

    const bool lfoReady = globalLFO.prepare(sampleRate);
    jassert(lfoReady);
    juce::ignoreUnused(lfoReady);
    globalLFO.setRateHz(TempoSync::rateHz(
        static_cast<int>(lfoSyncParameter->load(std::memory_order_relaxed)),
        TempoSync::fallbackBpm));
    globalLFO.setWaveform(static_cast<LFO::Waveform>(
        static_cast<int>(lfoWaveformParameter->load(std::memory_order_relaxed))));
    lfoScheduler.reset();
    lfoPhaseForUI.store(0.0f, std::memory_order_relaxed);

    constexpr double smoothingSeconds = 0.003;
    const float maxCutoff = juce::jmax(20.0f,
        juce::jmin(18000.0f, static_cast<float>(sampleRate * 0.45)));

    for (std::size_t i = 1; i < destinationSmoothers.size(); ++i)
    {
        const bool ready = destinationSmoothers[i].prepare(
            sampleRate, ControlRateScheduler::intervalSamples, smoothingSeconds);
        jassert(ready);
        juce::ignoreUnused(ready);
        const auto mapping = getMapping(static_cast<ModDestination>(i), maxCutoff);
        const auto initial = juce::jlimit(mapping.minimum, mapping.maximum, mapping.base);
        destinationSmoothers[i].reset(mapping.logarithmic ? std::log2(initial) : initial);
    }

    auto prepareBypass = [sampleRate](ModulationSmoother& smoother, float initial)
    {
        const bool ready = smoother.prepare(
            sampleRate, ControlRateScheduler::intervalSamples, 0.003);
        jassert(ready);
        juce::ignoreUnused(ready);
        smoother.reset(initial);
    };

    prepareBypass(filterBankBypassSmoother,
        filterBankEnabledParameter->load(std::memory_order_relaxed) > 0.5f ? 1.0f : 0.0f);
    prepareBypass(distortionBypassSmoother,
        distortionEnabledParameter->load(std::memory_order_relaxed) > 0.5f ? 1.0f : 0.0f);
    prepareBypass(chorusBypassSmoother,
        chorusEnabledParameter->load(std::memory_order_relaxed) > 0.5f ? 1.0f : 0.0f);

    // Start at the real mix (Mix x enabled), so a bypassed distortion does
    // not fade out audibly right after loading.
    distortionStage.prepare(spec,
        distortioDriveParameter->load(std::memory_order_relaxed),
        distortionEnabledParameter->load(std::memory_order_relaxed) > 0.5f
            ? distortioMixParameter->load(std::memory_order_relaxed) : 0.0f);
    // Oversampling delays the whole signal by a few samples: tell the host.
    setLatencySamples(distortionStage.getLatencySamples());

    outputGain.prepare(spec);
    outputGain.setRampDurationSeconds(0.02);
    outputGain.setGainDecibels(outputVolumeParameter->load(std::memory_order_relaxed));
    outputGain.reset();
    for (auto& peak : outputPeak)
        peak.store(0.0f, std::memory_order_relaxed);

    chorus.setRate(chorusRateParameter->load(std::memory_order_relaxed));
    chorus.setDepth(chorusDepthParameter->load(std::memory_order_relaxed));
    chorus.setCentreDelay(chorusDelayParameter->load(std::memory_order_relaxed));
    chorus.setMix(chorusEnabledParameter->load(std::memory_order_relaxed) > 0.5f
                      ? chorusMixParameter->load(std::memory_order_relaxed) : 0.0f);
    chorus.prepare(spec); // starts at the values above, without ramps

    modulatedPan = outputPanParameter->load(std::memory_order_relaxed);
    const auto pan = balanceGains(modulatedPan);
    panGainLeft = pan.first;
    panGainRight = pan.second;

    // processBlock() renders in 32-sample chunks and hands the synthesiser
    // only the events of each chunk (see chunkMidi), so events keep their
    // exact sample positions and are handled exactly once.
    chunkMidi.ensureSize(4096);
    synthesiser.setMinimumRenderingSubdivisionSize(1, false);
    synthesiser.setCurrentPlaybackSampleRate(sampleRate);
}

void ObjectSynthAudioProcessor::releaseResources(){}


bool ObjectSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const{
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}


void ObjectSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;

    const auto selectedWaveform = requestedWaveform.load(std::memory_order_relaxed);
    if (selectedWaveform != appliedWaveform)
    {
        for (int i = 0; i < synthesiser.getNumVoices(); ++i)
            static_cast<SynthVoice*>(synthesiser.getVoice(i))->setWaveform(selectedWaveform);
        appliedWaveform = selectedWaveform;
    }

    const juce::ADSR::Parameters amplitudeParameters {
        attackParameter->load(std::memory_order_relaxed),
        decayParameter->load(std::memory_order_relaxed),
        sustainParameter->load(std::memory_order_relaxed),
        releaseParameter->load(std::memory_order_relaxed)
    };
    const juce::ADSR::Parameters filterParameters {
        filterEnvAttackParameter->load(std::memory_order_relaxed),
        filterEnvDecayParameter->load(std::memory_order_relaxed),
        filterEnvSustainParameter->load(std::memory_order_relaxed),
        filterEnvReleaseParameter->load(std::memory_order_relaxed)
    };
    const bool oscillatorEnabled =
        oscillatorEnabledParameter->load(std::memory_order_relaxed) > 0.5f;
    const bool amplitudeEnvelopeEnabled =
        amplitudeEnvelopeEnabledParameter->load(std::memory_order_relaxed) > 0.5f;
    const bool unisonEnabled =
        unisonEnabledParameter->load(std::memory_order_relaxed) > 0.5f;
    const bool voiceFilterEnabled =
        filterEnvEnabledParameter->load(std::memory_order_relaxed) > 0.5f;

    const int unisonCount = juce::roundToInt(unisonCountParameter -> load(std::memory_order_relaxed));
    const float unisonDetune = unisonDetuneParameter -> load(std::memory_order_relaxed);
    const float unisonSpread = unisonSpreadParameter -> load(std::memory_order_relaxed);

    // Snapshot protocol (WavetableSnapshotStore.h): read nextId() first,
    // then the set. The table is derived from the set we loaded, so its
    // owner is known exactly and reported back at the end of the block.
    std::uint64_t oldestCustomSetInUse = customTableStore.nextId();
    const auto* customSet = currentCustomSet.load(std::memory_order_acquire);
    const BandLimitedWavetable* table = nullptr;
    const CustomTable* lfoTable = nullptr; // same frame, read raw by the LFO
    std::uint64_t customSetId = 0;
    if (customSet != nullptr && ! customSet->bandLimited.empty())
    {
        const int frame = juce::jlimit(0, static_cast<int>(customSet->bandLimited.size()) - 1,
                                       currentCustomFrame.load(std::memory_order_acquire));
        table = &customSet->bandLimited[static_cast<std::size_t>(frame)];
        if (static_cast<std::size_t>(frame) < customSet->frames.size())
            lfoTable = &customSet->frames[static_cast<std::size_t>(frame)];
        customSetId = customSet->id;
        oldestCustomSetInUse = customSetId;
    }

    for (int i = 0; i < synthesiser.getNumVoices(); ++i)
    {
        auto* voice = static_cast<SynthVoice*>(synthesiser.getVoice(i));
        voice -> setCustomWaveform(table, customSetId);
        voice -> setOscillatorEnabled(oscillatorEnabled);
        voice -> setAmplitudeEnvelopeEnabled(amplitudeEnvelopeEnabled);
        voice -> setUnisonEnabled(unisonEnabled);
        voice -> setADSRParameters(amplitudeParameters);
        voice -> setVoiceFilterEnabled(voiceFilterEnabled);
        voice -> setUnisonParameters(unisonCount, unisonDetune, unisonSpread);
    }

    // Read all APVTS values once per host block, on the audio thread.
    // LFO rate follows the host tempo; while the transport plays, the LFO
    // phase is also locked to the host's beat position so it stays on the grid.
    {
        const int syncIndex = static_cast<int>(lfoSyncParameter->load(std::memory_order_relaxed));
        double bpm = TempoSync::fallbackBpm;

        if (auto* playHead = getPlayHead())
        {
            if (const auto position = playHead->getPosition())
            {
                if (const auto hostBpm = position->getBpm())
                    bpm = *hostBpm;

                if (position->getIsPlaying())
                    if (const auto ppq = position->getPpqPosition())
                        globalLFO.reset(TempoSync::phaseAt(syncIndex, *ppq));
            }
        }

        globalLFO.setRateHz(TempoSync::rateHz(syncIndex, bpm));
    }
    globalLFO.setWaveform(static_cast<LFO::Waveform>(
        static_cast<int>(lfoWaveformParameter->load(std::memory_order_relaxed))));
    // Custom reads the oscillator's frame; the set stays alive for this block
    // (reported as in use below).
    globalLFO.setCustomTable(lfoTable != nullptr ? lfoTable->data() : nullptr,
                             lfoTable != nullptr ? lfoTable->size() : 0);

    const bool lfoEnabled = lfoEnabledParameter->load(std::memory_order_relaxed) > 0.5f;
    const bool filterBankEnabled = filterBankEnabledParameter->load(std::memory_order_relaxed) > 0.5f;
    const bool distortionEnabled = distortionEnabledParameter->load(std::memory_order_relaxed) > 0.5f;
    const bool chorusEnabled = chorusEnabledParameter->load(std::memory_order_relaxed) > 0.5f;

    for (std::size_t i = 0; i < ModulationMatrix::maxRoutes; ++i)
    {
        const int index = static_cast<int>(routeDestinationParameters[i]->load(std::memory_order_relaxed));
        const float depth = routeDepthParameters[i]->load(std::memory_order_relaxed);
        modulationMatrix.setRoute(i, {static_cast<ModDestination>(index), depth});
    }

    const float maxCutoff = juce::jmax(20.0f, juce::jmin(18000.0f, static_cast<float>(getSampleRate() * 0.45)));

    std::array<ModulationMapping, ModulationMatrix::destinationCount> mappings {};
    
    for (std::size_t i = 1; i < mappings.size(); ++i)
        mappings[i] = getMapping(static_cast<ModDestination>(i), maxCutoff);

    // Keyboard events use absolute offsets in the original MIDI buffer.
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    std::array<float, ModulationMatrix::destinationCount> effective {};

    const auto index = [](ModDestination destination) noexcept {
        return static_cast<std::size_t>(destination);
    };

    // Walks the (time-sorted) host MIDI once across all chunks.
    auto nextMidiEvent = midiMessages.cbegin();

    int offset = 0;
    while (offset < numSamples)
    {
        const int chunk = lfoScheduler.nextChunkSize(numSamples - offset);
        jassert(chunk > 0);
        if (chunk <= 0) break;

        if (lfoScheduler.isUpdateDue())
        {
            const float lfoSample = globalLFO.getNextSample();
            lfoPhaseForUI.store(static_cast<float>(globalLFO.getPhase()),
                                std::memory_order_relaxed);
            const auto modulation = lfoEnabled
                ? modulationMatrix.evaluate(lfoSample)
                : ModulationMatrix::Values {};

            for (std::size_t i = 1; i < effective.size(); ++i)
            {
                const auto& map = mappings[i];
                const float base = std::isfinite(map.base)
                    ? juce::jlimit(map.minimum, map.maximum, map.base) : map.minimum;
                const float target = map.logarithmic
                    ? base * std::exp2(map.excursion * modulation[i])
                    : base + map.excursion * modulation[i];
                const float safe = juce::jlimit(map.minimum, map.maximum, target);
                const float smoothInput = map.logarithmic ? std::log2(safe) : safe;
                const float smoothed = destinationSmoothers[i].processTarget(smoothInput);
                effective[i] = map.logarithmic ? std::exp2(smoothed) : smoothed;
                modulatedForUI[i].store(effective[i], std::memory_order_relaxed);
            }

            filterBank.setParameters(0, effective[index(ModDestination::LPCutoff)],
                                     effective[index(ModDestination::LPResonance)]);
            filterBank.setParameters(1, effective[index(ModDestination::HPCutoff)],
                                     effective[index(ModDestination::HPResonance)]);
            filterBank.setParameters(2, effective[index(ModDestination::BPCutoff)],
                                     effective[index(ModDestination::BPResonance)]);
            filterBank.setMix(effective[index(ModDestination::LPMix)],
                              effective[index(ModDestination::HPMix)],
                              effective[index(ModDestination::BPMix)]);
            filterBank.setOutputMix(filterBankBypassSmoother.processTarget(
                filterBankEnabled ? 1.0f : 0.0f));

            distortionStage.setDrive(effective[index(ModDestination::DistortionDrive)]);
            const float distortionWet = effective[index(ModDestination::DistortionMix)] *
                distortionBypassSmoother.processTarget(distortionEnabled ? 1.0f : 0.0f);
            distortionStage.setMix(distortionWet);

            chorus.setRate(effective[index(ModDestination::ChorusRate)]);
            chorus.setDepth(effective[index(ModDestination::ChorusDepth)]);
            chorus.setCentreDelay(effective[index(ModDestination::ChorusDelay)]);
            const float chorusWet = effective[index(ModDestination::ChorusMix)] *
                chorusBypassSmoother.processTarget(chorusEnabled ? 1.0f : 0.0f);
            chorus.setMix(chorusWet);
            modulatedPan = effective[index(ModDestination::OutputPan)];

            // Global LFO; each MIDI note still has its own Filter ADSR state.
            for (int i = 0; i < synthesiser.getNumVoices(); ++i)
            {
                auto* voice = static_cast<SynthVoice*>(synthesiser.getVoice(i));
                voice->setFilterEnvelopeParameters(
                    filterParameters,
                    effective[index(ModDestination::VoiceFilterCutoff)],
                    effective[index(ModDestination::FilterEnvelopeAmount)]);
            }

            globalLFO.advance(chunk - 1);
        }
        else
        {
            // Carry the audio-time phase across host-buffer boundaries.
            globalLFO.advance(chunk);
        }

        // Only this chunk's events: Synthesiser::renderNextBlock() would
        // otherwise fire later events early, once per chunk. The last chunk
        // also takes any event a host placed past the end of the buffer.
        chunkMidi.clear();
        const bool lastChunk = offset + chunk >= numSamples;
        for (; nextMidiEvent != midiMessages.cend(); ++nextMidiEvent)
        {
            const auto event = *nextMidiEvent;
            if (event.samplePosition >= offset + chunk && ! lastChunk)
                break;
            // Channel messages only: the synthesiser ignores SysEx, and
            // skipping it keeps the preallocated buffer allocation-free.
            if (event.numBytes <= 3)
                chunkMidi.addEvent(event.data, event.numBytes,
                                   juce::jmin(event.samplePosition, offset + chunk - 1));
        }

        // Processing synth + effects in the SAME chunks also enables
        // voice-filter and global-effect destinations, not just FilterBank.
        synthesiser.renderNextBlock(buffer, chunkMidi, offset, chunk);
        filterBank.process(buffer, offset, chunk);

        juce::dsp::AudioBlock<float> fullBlock(buffer);
        auto subBlock = fullBlock.getSubBlock(static_cast<std::size_t>(offset),
                                               static_cast<std::size_t>(chunk));

        distortionStage.process(subBlock);
        chorus.process(subBlock);

        // Output balance, ramped across the chunk so LFO auto-pan is smooth.
        if (buffer.getNumChannels() > 1)
        {
            const auto pan = balanceGains(modulatedPan);
            buffer.applyGainRamp(0, offset, chunk, panGainLeft, pan.first);
            buffer.applyGainRamp(1, offset, chunk, panGainRight, pan.second);
            panGainLeft = pan.first;
            panGainRight = pan.second;
        }

        lfoScheduler.advance(chunk);
        offset += chunk;
    }

    // Tell the message thread which custom wavetable snapshots may still be
    // read: the one loaded above, and those sounding voices play or are
    // still crossfading from.
    for (int i = 0; i < synthesiser.getNumVoices(); ++i)
    {
        const auto inUse = static_cast<SynthVoice*>(synthesiser.getVoice(i))->getCustomSetIdInUse();
        if (inUse != 0)
            oldestCustomSetInUse = std::min(oldestCustomSetInUse, inUse);
    }
    customTableStore.reportOldestInUse(oldestCustomSetInUse);

    // Output Volume (ramped), then the meter reads the final signal.
    outputGain.setGainDecibels(outputVolumeParameter->load(std::memory_order_relaxed));
    {
        juce::dsp::AudioBlock<float> block(buffer);
        outputGain.process(juce::dsp::ProcessContextReplacing<float>(block));
    }

    for (int channel = 0; channel < juce::jmin(2, buffer.getNumChannels()); ++channel)
    {
        const float blockPeak = buffer.getMagnitude(channel, 0, numSamples);
        auto& peak = outputPeak[static_cast<std::size_t>(channel)];
        float previous = peak.load(std::memory_order_relaxed);
        while (blockPeak > previous
               && ! peak.compare_exchange_weak(previous, blockPeak, std::memory_order_acq_rel))
        {
        }
    }
    if (buffer.getNumChannels() == 1) // mono layout: show the same level on both bars
    {
        const float mono = outputPeak[0].load(std::memory_order_relaxed);
        float previous = outputPeak[1].load(std::memory_order_relaxed);
        while (mono > previous
               && ! outputPeak[1].compare_exchange_weak(previous, mono, std::memory_order_acq_rel))
        {
        }
    }
}

void ObjectSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // APVTS state may contain properties from a previously restored preset.
    // Build metadata from the live custom set, never from stale serialized data.
    for (const auto* property : {"customWavetableData", "customWavetableName",
                                 "customFrameIndex", "customWavetableSeamWarning",
                                 "objectPhoto", "objectName"})
        state.removeProperty(juce::Identifier(property), nullptr);

    state.setProperty("selectedWaveform", static_cast<int>(getWaveform()), nullptr);

    {
        juce::String name;
        const auto photo = getObjectPhoto(name);
        if (! photo.isEmpty())
        {
            state.setProperty("objectPhoto", photo.toBase64Encoding(), nullptr);
            state.setProperty("objectName", name, nullptr);
        }
    }

    {
        std::lock_guard<std::mutex> lock(customTableMutex);
        const auto* set = currentCustomSet.load(std::memory_order_acquire);
        if (set != nullptr)
        {
            state.setProperty("customWavetableName", customWavetableName, nullptr);
            state.setProperty("customFrameIndex",
                              currentCustomFrame.load(std::memory_order_relaxed), nullptr);

            juce::MemoryOutputStream stream;
            stream.writeInt(2); // Multiframe preset format.
            stream.writeInt(static_cast<int>(set->frames.size()));
            for (std::size_t i = 0; i < set->frames.size(); ++i)
            {
                stream.writeByte(set->seamWarnings[i] ? 1 : 0);
                for (const float sample : set->frames[i])
                    stream.writeFloat(sample);
            }

            juce::MemoryBlock bytes(stream.getData(), stream.getDataSize());
            state.setProperty("customWavetableData", bytes.toBase64Encoding(), nullptr);
        }
    }

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

bool ObjectSynthAudioProcessor::parseEmbeddedWavetable(const juce::String& encoded,
                                                       bool legacySeamWarning,
                                                       std::vector<CustomTable>& frames,
                                                       std::vector<bool>& warnings)
{
    frames.clear();
    warnings.clear();

    juce::MemoryBlock bytes;
    if (! bytes.fromBase64Encoding(encoded) || bytes.getSize() < sizeof(juce::int32))
        return false;

    juce::MemoryInputStream stream(bytes, false);
    const int version = stream.readInt();
    int count = 0;
    bool validSize = false;

    if (version == 1)
    {
        // Compatibility with the previous single-cycle preset format.
        count = 1;
        validSize = bytes.getSize() ==
            sizeof(juce::int32) + WavetableBank::tableSize * sizeof(float);
    }
    else if (version == 2 && bytes.getSize() >= 2 * sizeof(juce::int32))
    {
        count = stream.readInt();
        if (count >= 1 && count <= 32)
        {
            const std::size_t perFrame = 1 +
                WavetableBank::tableSize * sizeof(float);
            validSize = bytes.getSize() ==
                2 * sizeof(juce::int32) + static_cast<std::size_t>(count) * perFrame;
        }
    }

    if (! validSize)
        return false;

    frames.reserve(static_cast<std::size_t>(count));
    warnings.reserve(static_cast<std::size_t>(count));
    bool valid = true;

    for (int i = 0; i < count; ++i)
    {
        const bool warning = version == 2 ? stream.readByte() != 0 : legacySeamWarning;
        CustomTable table{};
        float peak = 0.0f;
        for (float& sample : table)
        {
            sample = stream.readFloat();
            valid = valid && std::isfinite(sample);
            peak = std::max(peak, std::abs(sample));
        }
        valid = valid && peak > 1.0e-6f && peak <= 1.01f;
        frames.push_back(std::move(table));
        warnings.push_back(warning);
    }
    return valid;
}

void ObjectSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType()))
        return;
    auto restored = juce::ValueTree::fromXml(*xml);
    if (!restored.isValid())
        return;

    // Parse the embedded wavetable BEFORE touching the running synth, so new
    // notes never see a half-restored state and bad data never costs us the
    // current table.
    const auto encoded = restored.getProperty("customWavetableData").toString();
    const auto name = restored.getProperty("customWavetableName",
                                           "Restored custom wavetable").toString();
    const int frameIndex = static_cast<int>(restored.getProperty("customFrameIndex", 0));

    if (encoded.isEmpty())
    {
        // The preset has no custom wavetable: neither should the synth.
        // Old snapshots stay alive until no voice reads them.
        std::lock_guard<std::mutex> lock(customTableMutex);
        currentCustomSet.store(nullptr, std::memory_order_release);
        currentCustomTable.store(nullptr, std::memory_order_release);
        currentCustomFrame.store(0, std::memory_order_release);
        customSeamWarning.store(false, std::memory_order_release);
        customWavetableName.clear();
    }
    else
    {
        std::vector<CustomTable> frames;
        std::vector<bool> warnings;
        const bool legacyWarning = static_cast<bool>(
            restored.getProperty("customWavetableSeamWarning", false));

        if (parseEmbeddedWavetable(encoded, legacyWarning, frames, warnings))
        {
            // Hosts restore state often (project load, undo, preset browsing):
            // re-use the current snapshot when the data is identical.
            bool identical = false;
            {
                std::lock_guard<std::mutex> lock(customTableMutex);
                const auto* current = currentCustomSet.load(std::memory_order_acquire);
                identical = current != nullptr && current->frames == frames
                            && current->seamWarnings == warnings;
                if (identical)
                    customWavetableName = name;
            }

            if (identical)
                selectCustomFrame(juce::jlimit(0, static_cast<int>(frames.size()) - 1, frameIndex));
            else
                publishCustomFrames(std::move(frames), std::move(warnings), name, frameIndex);
        }
        // Invalid data: keep the current wavetable instead of losing it.
    }

    const int waveform = juce::jlimit(
        0, static_cast<int>(Waveform::Custom),
        static_cast<int>(restored.getProperty("selectedWaveform", 1)));

    {
        // Object photo of a camera preset; other presets clear it.
        juce::MemoryBlock photo;
        const auto encodedPhoto = restored.getProperty("objectPhoto").toString();
        if (encodedPhoto.isEmpty() || ! photo.fromBase64Encoding(encodedPhoto)
            || photo.getSize() > 512 * 1024)
            photo.reset();
        const auto photoName = photo.isEmpty() ? juce::String()
                                               : restored.getProperty("objectName").toString();
        setObjectPhoto(std::move(photo), photoName);
    }

    // These fields belong to the immutable wavetable snapshots, not APVTS.
    // Avoid keeping a large, possibly stale Base64 blob inside APVTS state.
    for (const auto* property : {"customWavetableData", "customWavetableName",
                                 "customFrameIndex", "customWavetableSeamWarning",
                                 "objectPhoto", "objectName"})
        restored.removeProperty(juce::Identifier(property), nullptr);

    // A preset saved before a parameter existed (e.g. outputVolume,
    // outputPan, unisonSpread) has no entry for it, and APVTS would then
    // keep that parameter's CURRENT value. Restore it at its default instead.
    for (auto* parameter : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr || restored.getChildWithProperty("id", ranged->paramID).isValid())
            continue;
        juce::ValueTree entry("PARAM");
        entry.setProperty("id", ranged->paramID, nullptr);
        entry.setProperty("value", ranged->convertFrom0to1(ranged->getDefaultValue()), nullptr);
        restored.appendChild(entry, nullptr);
    }

    apvts.replaceState(restored);
    setWaveform(waveform == static_cast<int>(Waveform::Custom) &&
                        currentCustomTable.load(std::memory_order_acquire) == nullptr
                    ? Waveform::Sawtooth
                    : static_cast<Waveform>(waveform));
}

juce::AudioProcessorEditor* ObjectSynthAudioProcessor::createEditor(){
    return new ObjectSynthAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){
    return new ObjectSynthAudioProcessor();
}


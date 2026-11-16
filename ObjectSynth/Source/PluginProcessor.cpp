#include <cmath>
#include <algorithm>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Synth/SynthVoice.h"
#include "Synth/SynthSound.h"

juce::AudioProcessorValueTreeState::ParameterLayout 
ObjectSynthAudioProcessor::createParameterLayout(){

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"attack", 1}, "Attack", juce::NormalisableRange<float>(0.0f, 5.0f, 0.001f), 0.1f
    ));
    
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"decay", 1}, "Decay", juce::NormalisableRange<float>(0.0f, 5.0f, 0.001f), 0.1f
    ));
    
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"sustain", 1}, "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.7f
    ));
    
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"release", 1}, "Release", juce::NormalisableRange<float>(0.0f, 5.0f, 0.001f), 0.2f
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lpCutoff",1}, "LP Cutoff", juce::NormalisableRange<float>(20.0f, 18000.0f, 1.0f, 0.3f), 800.0f
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
        juce::ParameterID{"filterEnvAmount", 1}, "Filter Env Amount", juce::NormalisableRange<float>{-4.0f, 4.0f, 0.1f}, 3.0f
    ));

    // Module bypass is deliberately independent from Dry/Wet and Mix.
    for (const auto* id : {"filterBankEnabled", "filterEnvEnabled",
                           "distortionEnabled", "chorusEnabled", "lfoEnabled"})
    {
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID {id, 1}, juce::String(id), true));
    }

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID {"lfoRate", 1}, "LFO Rate",
        juce::NormalisableRange<float> {0.05f, 20.0f, 0.01f, 0.35f}, 10.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"lfoWaveform", 1}, "LFO Waveform",
        juce::StringArray {"Sine", "Saw", "Square", "Triangle"}, 0));

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
    
    lfoEnabledParameter = apvts.getRawParameterValue("lfoEnabled");
    lfoRateParameter = apvts.getRawParameterValue("lfoRate");
    lfoWaveformParameter = apvts.getRawParameterValue("lfoWaveform");
    filterBankEnabledParameter = apvts.getRawParameterValue("filterBankEnabled");
    filterEnvEnabledParameter = apvts.getRawParameterValue("filterEnvEnabled");
    distortionEnabledParameter = apvts.getRawParameterValue("distortionEnabled");
    chorusEnabledParameter = apvts.getRawParameterValue("chorusEnabled");

    jassert(lfoEnabledParameter && lfoRateParameter && lfoWaveformParameter);
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

    distortion.functionToUse = [](float x){
        return std::tanh(x);
    };

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

        case ModDestination::None:
        case ModDestination::Count: return {};
    }
    return {};
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
    globalLFO.setRateHz(lfoRateParameter->load(std::memory_order_relaxed));
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

    distortionDrive.prepare(spec);
    distortionDrive.setRampDurationSeconds(0.02);
    distortionDrive.setGainLinear(distortioDriveParameter->load(std::memory_order_relaxed));
    distortion.prepare(spec);
    distortion.reset();
    distortionMixer.prepare(spec);
    distortionMixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    distortionMixer.setWetMixProportion(distortioMixParameter->load(std::memory_order_relaxed));

    chorus.prepare(spec);
    chorus.setRate(chorusRateParameter->load(std::memory_order_relaxed));
    chorus.setDepth(chorusDepthParameter->load(std::memory_order_relaxed));
    chorus.setCentreDelay(chorusDelayParameter->load(std::memory_order_relaxed));
    chorus.setFeedback(0.0f);
    chorus.setMix(chorusMixParameter->load(std::memory_order_relaxed));
    chorus.reset();

    // MIDI events keep their positions when audio is rendered in 32-sample chunks.
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
    const bool voiceFilterEnabled =
        filterEnvEnabledParameter->load(std::memory_order_relaxed) > 0.5f;

    for (int i = 0; i < synthesiser.getNumVoices(); ++i)
    {
        auto* voice = static_cast<SynthVoice*>(synthesiser.getVoice(i));
        voice->setADSRParameters(amplitudeParameters);
        voice->setVoiceFilterEnabled(voiceFilterEnabled);
    }

    // Read all APVTS values once per host block, on the audio thread.
    globalLFO.setRateHz(lfoRateParameter->load(std::memory_order_relaxed));
    globalLFO.setWaveform(static_cast<LFO::Waveform>(
        static_cast<int>(lfoWaveformParameter->load(std::memory_order_relaxed))));

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

    const float maxCutoff = juce::jmax(20.0f,
        juce::jmin(18000.0f, static_cast<float>(getSampleRate() * 0.45)));
    std::array<ModulationMapping, ModulationMatrix::destinationCount> mappings {};
    for (std::size_t i = 1; i < mappings.size(); ++i)
        mappings[i] = getMapping(static_cast<ModDestination>(i), maxCutoff);

    // Keyboard events use absolute offsets in the original MIDI buffer.
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    std::array<float, ModulationMatrix::destinationCount> effective {};
    const auto index = [](ModDestination destination) noexcept
    {
        return static_cast<std::size_t>(destination);
    };

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

            distortionDrive.setGainLinear(effective[index(ModDestination::DistortionDrive)]);
            const float distortionWet = effective[index(ModDestination::DistortionMix)] *
                distortionBypassSmoother.processTarget(distortionEnabled ? 1.0f : 0.0f);
            distortionMixer.setWetMixProportion(juce::jlimit(0.0f, 1.0f, distortionWet));

            chorus.setRate(effective[index(ModDestination::ChorusRate)]);
            chorus.setDepth(effective[index(ModDestination::ChorusDepth)]);
            chorus.setCentreDelay(effective[index(ModDestination::ChorusDelay)]);
            const float chorusWet = effective[index(ModDestination::ChorusMix)] *
                chorusBypassSmoother.processTarget(chorusEnabled ? 1.0f : 0.0f);
            chorus.setMix(juce::jlimit(0.0f, 1.0f, chorusWet));

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

        // Processing synth + effects in the SAME chunks also enables
        // voice-filter and global-effect destinations, not just FilterBank.
        synthesiser.renderNextBlock(buffer, midiMessages, offset, chunk);
        filterBank.process(buffer, offset, chunk);

        juce::dsp::AudioBlock<float> fullBlock(buffer);
        auto subBlock = fullBlock.getSubBlock(static_cast<std::size_t>(offset),
                                               static_cast<std::size_t>(chunk));
        juce::dsp::ProcessContextReplacing<float> context(subBlock);

        distortionMixer.pushDrySamples(juce::dsp::AudioBlock<const float>(subBlock));
        distortionDrive.process(context);
        distortion.process(context);
        distortionMixer.mixWetSamples(subBlock);
        chorus.process(context);

        lfoScheduler.advance(chunk);
        offset += chunk;
    }

    // Preserve the original master output attenuation.
    buffer.applyGain(0.5f);
}

void ObjectSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("selectedWaveform", static_cast<int>(getWaveform()), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void ObjectSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            auto restored = juce::ValueTree::fromXml(*xml);
            const int waveform = juce::jlimit(0, 3,
                static_cast<int>(restored.getProperty("selectedWaveform", 1)));
            apvts.replaceState(restored);
            setWaveform(static_cast<Waveform>(waveform));
        }
    }
}

juce::AudioProcessorEditor* ObjectSynthAudioProcessor::createEditor(){
    return new ObjectSynthAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){
    return new ObjectSynthAudioProcessor();
}


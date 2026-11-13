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
    
    distortion.functionToUse = [](float x){
        return std::tanh(x);
    };

    for (int i = 0; i < 8; ++i){
        synthesiser.addVoice(new SynthVoice());
    }

    synthesiser.addSound(new SynthSound());
}


void ObjectSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock){

    juce::dsp::ProcessSpec spec{
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        static_cast<juce::uint32>(getTotalNumOutputChannels()),
    };

    filterBank.prepare(spec);

    distortionDrive.prepare(spec);
    distortionDrive.setRampDurationSeconds(0.02);
    distortionDrive.setGainLinear(3.0f);

    distortion.prepare(spec);
    distortion.reset();
    
    distortionMixer.prepare(spec);
    distortionMixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    distortionMixer.setWetMixProportion(1.0f);

    
    chorus.prepare(spec);

    chorus.setRate(0.8f);
    chorus.setDepth(0.3f);
    chorus.setCentreDelay(7.0f);
    chorus.setFeedback(0.0f);
    chorus.setMix(0.4f);

    chorus.reset();


    synthesiser.setCurrentPlaybackSampleRate(sampleRate);

}


void ObjectSynthAudioProcessor::releaseResources(){}


bool ObjectSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const{
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}


void ObjectSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages){

    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    const Waveform selectedWaveform = requestedWaveform.load(std::memory_order_relaxed);

    if (selectedWaveform != appliedWaveform){
    
        for (int i = 0; i < synthesiser.getNumVoices(); ++i){
            auto* voice = static_cast<SynthVoice*>(synthesiser.getVoice(i));
            voice -> setWaveform(selectedWaveform);
        }

        appliedWaveform = selectedWaveform;
    }

    juce::ADSR::Parameters parameters {
        attackParameter->load(std::memory_order_relaxed),
        decayParameter->load(std::memory_order_relaxed),
        sustainParameter->load(std::memory_order_relaxed),
        releaseParameter->load(std::memory_order_relaxed),
    };

    for (int i = 0; i < synthesiser.getNumVoices(); ++i){
        auto* voice = static_cast<SynthVoice*>(synthesiser.getVoice(i));
        voice -> setADSRParameters(parameters);
    }

    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);
    synthesiser.renderNextBlock(buffer, midiMessages, 0, numSamples);
     
    const float maxCutoff = static_cast<float>(getSampleRate()*0.45);

    filterBank.setParameters(
        0, 
        juce::jmin(lpCutoffParameter -> load(std::memory_order_relaxed), maxCutoff),
        lpResonanceParameter -> load(std::memory_order_relaxed)
    );

    filterBank.setParameters(
        1, 
        juce::jmin(hpCutoffParameter -> load(std::memory_order_relaxed), maxCutoff),
        hpResonanceParameter -> load(std::memory_order_relaxed)
    );

    filterBank.setParameters(
        2, 
        juce::jmin(bpCutoffParameter -> load(std::memory_order_relaxed), maxCutoff),
        bpResonanceParameter -> load(std::memory_order_relaxed)
    );
    
    filterBank.setMix(
        lpMixParameter -> load(std::memory_order_relaxed),
        hpMixParameter -> load(std::memory_order_relaxed),
        bpMixParameter -> load(std::memory_order_relaxed)
    );

    filterBank.process(buffer);

    const float drive = distortioDriveParameter -> load(std::memory_order_relaxed);
    const float wet = distortioMixParameter -> load(std::memory_order_relaxed);

    distortionDrive.setGainLinear(drive);
    distortionMixer.setWetMixProportion(wet);

    juce::dsp::AudioBlock<float> audioBlock(buffer);
    juce::dsp::ProcessContextReplacing<float> context(audioBlock);

    distortionMixer.pushDrySamples(juce::dsp::AudioBlock<const float>(audioBlock));
    distortionDrive.process(context);
    distortion.process(context);
    distortionMixer.mixWetSamples(audioBlock);

    
    chorus.setRate(chorusRateParameter->load(std::memory_order_relaxed));
    chorus.setDepth(chorusDepthParameter->load(std::memory_order_relaxed));
    chorus.setCentreDelay(chorusDelayParameter->load(std::memory_order_relaxed));
    chorus.setMix(chorusMixParameter->load(std::memory_order_relaxed));
    chorus.process(context);

    // Note-on/note-off from the MIDI keyboard (or host MIDI) control sound.
    // Keep the existing output attenuation, without a global Play/Stop gate.
    buffer.applyGain(0.5f);
}

juce::AudioProcessorEditor* ObjectSynthAudioProcessor::createEditor(){
    return new ObjectSynthAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){
    return new ObjectSynthAudioProcessor();
}


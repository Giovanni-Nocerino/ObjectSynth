#include <algorithm>
#include <cmath>
#include <utility>
#include "SynthVoice.h"
#include "SynthSound.h"
#include "WavetableOscillator.h"


bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound){
    return sound != nullptr;
}

// ------------------------------------------------------------ live settings
// Each setter stores the value; if a note is sounding it is applied at once,
// with a transition (see the class comment). Unchanged values cost nothing.

void SynthVoice::setWaveform(Waveform newWaveform) noexcept
{
    if (newWaveform == waveform)
        return;
    waveform = newWaveform;
    if (isVoiceActive())
        applyTables();
}

void SynthVoice::setCustomWaveform(const BandLimitedWavetable* table, std::uint64_t setId) noexcept
{
    if (table == customTable && setId == customSetId)
        return;
    customTable = table;
    customSetId = setId;
    if (isVoiceActive())
        applyTables();
}

std::uint64_t SynthVoice::getCustomSetIdInUse() const noexcept
{
    if (!isVoiceActive())
        return 0;
    std::uint64_t id = latchedCustomSetId;
    if (fadingCustomSamples > 0 && fadingCustomSetId != 0)
        id = id == 0 ? fadingCustomSetId : std::min(id, fadingCustomSetId);
    return id;
}

void SynthVoice::setADSRParameters(const juce::ADSR::Parameters& newParameters) noexcept
{
    amplitudeParameters = newParameters;
    if (isVoiceActive())
        applyAmplitudeEnvelopeParameters();
}

void SynthVoice::setAmplitudeEnvelopeEnabled(bool enabled) noexcept
{
    if (enabled == amplitudeEnvelopeEnabled)
        return;
    amplitudeEnvelopeEnabled = enabled;
    if (isVoiceActive())
        applyAmplitudeEnvelopeParameters();
}

void SynthVoice::setFilterEnvelopeParameters(const juce::ADSR::Parameters& parameters,
                                             float baseCutoff, float amountOctaves) noexcept
{
    filterParameters = parameters;
    voiceFilterBaseCutoff = baseCutoff;
    voiceFilterAmountOctaves = amountOctaves;
    if (isVoiceActive())
        filterEnvelope.setParameters(parameters.attack, parameters.decay,
                                     parameters.sustain, parameters.release);
}

void SynthVoice::setUnisonEnabled(bool enabled) noexcept
{
    if (enabled == unisonEnabled)
        return;
    unisonEnabled = enabled;
    if (isVoiceActive())
        applyUnison(true);
}

void SynthVoice::setUnisonParameters(int count, float newDetuneCents, float newSpread) noexcept
{
    count = juce::jlimit(1, maxUnisonOscillators, count);
    newDetuneCents = std::isfinite(newDetuneCents) ? juce::jlimit(0.0f, 25.0f, newDetuneCents) : detuneCents;
    newSpread = std::isfinite(newSpread) ? juce::jlimit(0.0f, 1.0f, newSpread) : spread;
    if (count == unisonCount && newDetuneCents == detuneCents && newSpread == spread)
        return;
    unisonCount = count;
    detuneCents = newDetuneCents;
    spread = newSpread;
    if (isVoiceActive())
        applyUnison(true);
}

// ---------------------------------------------------------------- applying

void SynthVoice::applyTables() noexcept
{
    const std::uint64_t newId = waveform == Waveform::Custom && customTable != nullptr ? customSetId : 0;
    if (newId != latchedCustomSetId)
    {
        // The oscillators now crossfade OUT of the old snapshot: keep it alive
        // (reported to the processor) until the fade is over.
        if (latchedCustomSetId != 0)
        {
            fadingCustomSetId = latchedCustomSetId;
            fadingCustomSamples = transitionSamples;
        }
        latchedCustomSetId = newId;
    }

    for (std::size_t i = 0; i < oscillators.size(); ++i)
    {
        oscillators[i].setWaveform(waveform);
        oscillators[i].setCustomTable(customTable);
        if (!rendering[i])
            oscillators[i].snapToCurrentTable(); // silent: nothing to fade from
    }
}

void SynthVoice::applyUnison(bool ramp) noexcept
{
    activeUnisonCount = unisonEnabled ? unisonCount : 1;

    for (int i = 0; i < maxUnisonOscillators; ++i)
    {
        const auto index = static_cast<std::size_t>(i);
        auto& oscillator = oscillators[index];
        const bool active = i < activeUnisonCount;

        if (active)
        {
            // Detune and stereo position follow the same -1..+1 position.
            // Changing the frequency keeps the phase, so it never clicks.
            const double position = activeUnisonCount == 1 ? 0.0
                : 2.0 * static_cast<double>(i) / static_cast<double>(activeUnisonCount - 1) - 1.0;
            oscillator.setFrequency(noteFrequency * std::exp2(position * static_cast<double>(detuneCents) / 1200.0));

            // Equal-power pan law, scaled so a centred oscillator keeps gain 1.
            const double angle = (1.0 + position * static_cast<double>(spread))
                                 * juce::MathConstants<double>::pi / 4.0;
            const auto left = static_cast<float>(std::sqrt(2.0) * std::cos(angle));
            const auto right = static_cast<float>(std::sqrt(2.0) * std::sin(angle));

            if (!rendering[index])
            {
                // Joins from silence: no crossfade, pan already in place,
                // and its level fades in (or starts at once for a new note).
                oscillator.snapToCurrentTable();
                panLeft[index].setCurrentAndTargetValue(left);
                panRight[index].setCurrentAndTargetValue(right);
                oscillatorGain[index].setCurrentAndTargetValue(ramp ? 0.0f : 1.0f);
                oscillatorGain[index].setTargetValue(1.0f);
                rendering[index] = true;
            }
            else if (ramp)
            {
                panLeft[index].setTargetValue(left);
                panRight[index].setTargetValue(right);
                oscillatorGain[index].setTargetValue(1.0f);
            }
            else
            {
                panLeft[index].setCurrentAndTargetValue(left);
                panRight[index].setCurrentAndTargetValue(right);
                oscillatorGain[index].setCurrentAndTargetValue(1.0f);
            }
        }
        else if (ramp)
        {
            oscillatorGain[index].setTargetValue(0.0f); // fades out, then stops rendering
        }
        else
        {
            oscillatorGain[index].setCurrentAndTargetValue(0.0f);
        }
    }

    // Detuned oscillators add up like uncorrelated signals (power), so the
    // sum is divided by sqrt(N), not N: 7 voices are as loud as 1.
    const float normalisation = 1.0f / std::sqrt(static_cast<float>(activeUnisonCount));
    if (ramp)
        unisonNormalisation.setTargetValue(normalisation);
    else
        unisonNormalisation.setCurrentAndTargetValue(normalisation);
}

void SynthVoice::applyAmplitudeEnvelopeParameters() noexcept
{
    // Bypassing ADSR means a constant-level note with short anti-click
    // transitions. The note still ends correctly on MIDI Note Off.
    const juce::ADSR::Parameters gated {0.003f, 0.003f, 1.0f, 0.003f};
    const auto& p = amplitudeEnvelopeEnabled ? amplitudeParameters : gated;
    envelope.setParameters(p.attack, p.decay, p.sustain, p.release);
}

int SynthVoice::oscillatorsToRender() noexcept
{
    int count = 0;
    for (int i = 0; i < maxUnisonOscillators; ++i)
    {
        const auto index = static_cast<std::size_t>(i);
        const auto& gain = oscillatorGain[index];
        const bool audible = gain.getTargetValue() > 0.0f || gain.isSmoothing()
                             || gain.getCurrentValue() > 0.0f;
        if (audible)
        {
            rendering[index] = true;
            count = i + 1;
        }
        else if (rendering[index])
        {
            // Faded out: stop reading it, and drop any table it was fading
            // from (that custom snapshot may be freed while it is silent).
            rendering[index] = false;
            oscillators[index].snapToCurrentTable();
        }
    }
    return count;
}

// -------------------------------------------------------------- note events

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int pitchWheelPosition){

    juce::ignoreUnused(sound, pitchWheelPosition);

    // juce::Synthesiser steals a voice by calling stopNote(..., false) and
    // then startNote() straight away. Resetting envelopes and filter here
    // would drop the old note to zero in one sample (an audible click), so a
    // stolen voice keeps them: the attack starts from the current level, the
    // oscillator phases continue and their tables crossfade.
    const bool stolen = std::exchange(retriggerFromCurrentLevel, false);

    noteFrequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);

    if (!stolen)
    {
        // Fresh note on a silent voice: no transitions from a previous note.
        rendering = {};
        latchedCustomSetId = 0;
        fadingCustomSetId = 0;
        fadingCustomSamples = 0;
    }

    applyTables();
    applyUnison(stolen);

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

    applyAmplitudeEnvelopeParameters();
    envelope.noteOn();

    filterEnvelope.setParameters(filterParameters.attack, filterParameters.decay,
                                 filterParameters.sustain, filterParameters.release);
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

// ---------------------------------------------------------------- rendering

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
    const int oscillatorCount = oscillatorsToRender();

    for (int sample = 0; sample < numSamples; ++sample){

        const float envelopeValue = envelope.getNextSample();

        if (!envelope.isActive()){
            clearCurrentNote();
            break;
        }

        const float filterEnvValue = filterEnvelope.getNextSample();

        float left = 0.0f;
        float right = 0.0f;
        for (int osc = 0; osc < oscillatorCount; ++osc){
            const auto index = static_cast<std::size_t>(osc);
            if (!rendering[index])
                continue;
            const float value = oscillators[index].getNextSample()
                              * oscillatorGain[index].getNextValue();
            left += value * panLeft[index].getNextValue();
            right += value * panRight[index].getNextValue();
        }
        const float normalisation = unisonNormalisation.getNextValue();
        left *= normalisation;
        right *= normalisation;

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

    // The oscillators have advanced through (part of) their crossfade.
    fadingCustomSamples = std::max(0, fadingCustomSamples - numSamples);
}


void SynthVoice::setCurrentPlaybackSampleRate(
    double newSampleRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate(
        newSampleRate
    );

    if (newSampleRate <= 0.0)
        return;

    transitionSamples = juce::jmax(1, juce::roundToInt(transitionSeconds * newSampleRate));

    // Spread the start phases (golden-ratio steps): unison oscillators that
    // all start at phase 0 add up coherently into a loud first transient.
    for (std::size_t i = 0; i < oscillators.size(); ++i){
        oscillators[i].prepare(newSampleRate);
        oscillators[i].setPhase(0.6180339887 * static_cast<double>(i));
        oscillators[i].setTableCrossfade(transitionSamples);

        oscillatorGain[i].reset(newSampleRate, transitionSeconds);
        oscillatorGain[i].setCurrentAndTargetValue(0.0f);
        panLeft[i].reset(newSampleRate, transitionSeconds);
        panLeft[i].setCurrentAndTargetValue(1.0f);
        panRight[i].reset(newSampleRate, transitionSeconds);
        panRight[i].setCurrentAndTargetValue(1.0f);
    }
    rendering = {};
    unisonNormalisation.reset(newSampleRate, transitionSeconds);
    unisonNormalisation.setCurrentAndTargetValue(1.0f);
    latchedCustomSetId = 0;
    fadingCustomSetId = 0;
    fadingCustomSamples = 0;

    envelope.setSampleRate(newSampleRate);
    envelope.reset();
    applyAmplitudeEnvelopeParameters();

    filterEnvelope.setSampleRate(newSampleRate);
    filterEnvelope.reset();
    filterEnvelope.setParameters(filterParameters.attack, filterParameters.decay,
                                 filterParameters.sustain, filterParameters.release);

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

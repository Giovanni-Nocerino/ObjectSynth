#include "WavetableOscillator.h"

#include <cmath>
#include <stdexcept>


void WavetableOscillator::prepare(double newSampleRate){
    
    if (newSampleRate <= 0.0)
        throw std::invalid_argument("Invalid Sample Rate");
    
    sampleRate = newSampleRate;
    readPosition = 0.0;
    readIncrement = frequency * static_cast<double>(getSize()) / sampleRate; 
    updateTable();
}


void WavetableOscillator::setFrequency(double newFrequency){
    
    if (newFrequency < 0.0)
    throw std::invalid_argument("Invalid Frequency");
    
    frequency = newFrequency;
    readIncrement = frequency * static_cast<double>(getSize()) / sampleRate; 
    updateTable();
}


float WavetableOscillator::getNextSample() noexcept {

    const auto& samples = *table; // band-limited for this frequency (updateTable)

    const std::size_t index0 = static_cast<std::size_t>(readPosition);
    const std::size_t index1 = (index0 + 1) % samples.size();

    const double alpha = readPosition - index0;
    float sample = static_cast<float>(samples[index0]*(1.0 - alpha) + samples[index1]*alpha);

    // Crossfade after a table change: same phase, weights move linearly from
    // the old table to the new one, so the waveform morphs instead of jumping.
    if (fadeRemaining > 0)
    {
        const auto& old = *previousTable;
        const float oldSample = static_cast<float>(old[index0]*(1.0 - alpha) + old[index1]*alpha);
        const float oldWeight = static_cast<float>(fadeRemaining) / static_cast<float>(crossfadeLength);
        sample += oldWeight * (oldSample - sample);
        if (--fadeRemaining == 0)
            previousTable = nullptr;
    }

    readPosition += readIncrement;

    if(readPosition >= samples.size())
        readPosition = std::fmod(readPosition, static_cast<double>(samples.size()));

    return sample; 
}
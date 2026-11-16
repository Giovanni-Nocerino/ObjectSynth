#include "WavetableOscillator.h"

#include <cmath>
#include <stdexcept>


void WavetableOscillator::prepare(double newSampleRate){
    
    if (newSampleRate <= 0.0)
        throw std::invalid_argument("Invalid Sample Rate");
    
    sampleRate = newSampleRate;
    readPosition = 0.0;
    readIncrement = frequency * static_cast<double>(getSize()) / sampleRate; 
}


void WavetableOscillator::setFrequency(double newFrequency){
    
    if (newFrequency < 0.0)
    throw std::invalid_argument("Invalid Frequency");
    
    frequency = newFrequency;
    readIncrement = frequency * static_cast<double>(getSize()) / sampleRate; 
}


float WavetableOscillator::getNextSample() noexcept {

    const auto& samples = waveform == Waveform::Custom && customTable != nullptr ? *customTable : bank -> getTable(waveform);

    const std::size_t index0 = static_cast<std::size_t>(readPosition);
    const std::size_t index1 = (index0 + 1) % samples.size();

    const double alpha = readPosition - index0;
    const float sample = static_cast<float>(samples[index0]*(1.0 - alpha) + samples[index1]*alpha);

    readPosition += readIncrement;

    if(readPosition >= samples.size())
        readPosition = std::fmod(readPosition, static_cast<double>(samples.size()));

    return sample; 
}
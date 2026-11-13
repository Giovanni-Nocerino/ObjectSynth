#include "WavetableOscillator.h"

#include <cmath>
#include <stdexcept>


WavetableOscillator::WavetableOscillator(std::size_t size){
    
    if (size < 2)
        throw std::invalid_argument("Wavetable size must be at least 2");

    for (auto& table : waveTable){
        table.resize(size);
    }

    constexpr float twoPi = 6.283185307179586f;

    for (std::size_t i = 0; i < size; ++i){

        const float phase = static_cast<float>(i) / static_cast<float>(size);
        
        waveTable[0][i] = std::sin(twoPi*phase);
        waveTable[1][i] = 2.0f * phase - 1.0f;
        waveTable[2][i] = phase < 0.5f ? -1.0f : 1.0f;
        waveTable[3][i] = 1.0f - 4.0f*std::abs(phase - 0.5f);
    }
}

std::size_t WavetableOscillator::getSize() const noexcept {
    return waveTable[0].size();
}


void WavetableOscillator::prepare(double newSampleRate){
    
    if (newSampleRate <= 0.0)
        throw std::invalid_argument("Invalid Sample Rate");
    
    sampleRate = newSampleRate;
    readPosition = 0.0;
    readIncrement = frequency * waveTable[0].size() / sampleRate; 
}


void WavetableOscillator::setFrequency(double newFrequency){
    
    if (newFrequency < 0.0)
    throw std::invalid_argument("Invalid Frequency");
    
    frequency = newFrequency;
    readIncrement = frequency * waveTable[0].size() / sampleRate; 
}


float WavetableOscillator::getNextSample() noexcept {

    const auto& samples = waveTable[static_cast<std::size_t>(waveform)];

    const std::size_t index0 = static_cast<std::size_t>(readPosition);
    const std::size_t index1 = (index0 + 1) % samples.size();

    const double alpha = readPosition - index0;
    const float sample = static_cast<float>(samples[index0]*(1.0 - alpha) + samples[index1]*alpha);

    readPosition += readIncrement;

    if(readPosition >= samples.size())
        readPosition = std::fmod(readPosition, static_cast<double>(samples.size()));

    return sample; 
}
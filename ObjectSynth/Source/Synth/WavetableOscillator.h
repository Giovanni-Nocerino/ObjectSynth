#pragma once

#include <vector>
#include <array>

enum class Waveform{
    Sine,
    Sawtooth,
    Square,
    Triangle
};

class WavetableOscillator {
    public:
        explicit WavetableOscillator(std::size_t size = 2048);

        std::size_t getSize() const noexcept;
        
        void prepare(double newSampleRate);
        
        void setFrequency(double newFrequency);

        float getNextSample() noexcept;

        void setWaveform(Waveform newWaveform) noexcept{
            waveform = newWaveform;
        }

    private:
        std::array<std::vector<float>,4> waveTable;
        Waveform waveform = Waveform::Sawtooth;
        
        double sampleRate = 48000.0;
        double frequency = 0.0;

        double readPosition = 0.0;
        double readIncrement = 0.0;

};
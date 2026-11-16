#pragma once

#include "WavetableBank.h"
#include <cmath>
#include <cstddef>

class WavetableOscillator {
public:
    WavetableOscillator() noexcept = default;

    explicit WavetableOscillator(const WavetableBank& data) noexcept : bank(&data) {}

    std::size_t getSize() const noexcept {
        return WavetableBank::tableSize;
    }

    void prepare(double newSampleRate);

    void setFrequency(double newFrequency);

    float getNextSample() noexcept;

    void setWaveform(Waveform newWaveform) noexcept {
        waveform = newWaveform;
    }

    void resetPhase() noexcept {
        readPosition = 0.0;
    }

    // Phase as a fraction of one cycle, [0, 1).
    void setPhase(double cycles) noexcept {
        const double wrapped = cycles - std::floor(cycles);
        readPosition = std::isfinite(wrapped) ? wrapped * static_cast<double>(getSize()) : 0.0;
    }

    void setCustomTable(const WavetableBank::Table* table) noexcept{
        customTable = table;
    }

private:
    const WavetableBank* bank = &WavetableBank::builtIn();
    const WavetableBank::Table* customTable = nullptr;

    Waveform waveform = Waveform::Sawtooth;

    double sampleRate = 48000.0;
    double frequency = 0.0;

    double readPosition = 0.0;
    double readIncrement = 0.0;
};

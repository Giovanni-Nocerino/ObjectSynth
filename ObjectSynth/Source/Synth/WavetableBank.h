#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

#include "BandLimitedWavetable.h"

enum class Waveform {
    Sine,
    Sawtooth,
    Square,
    Triangle,
    Custom
};

class WavetableBank final {
public:
    static constexpr std::size_t tableSize = 2048;
    
    using Table = std::array<float, tableSize>;
    static_assert(std::is_same_v<Table, BandLimitedWavetable::Table>,
                  "band-limited levels must use the same table type");

    WavetableBank() noexcept {
        constexpr float twoPi = 6.283185307179586f;
        for (std::size_t i = 0; i < tableSize; ++i){

            const float phase = static_cast<float>(i)/static_cast<float>(tableSize);
            tables[0][i] = std::sin(twoPi * phase);
            tables[1][i] = 2.0f * phase - 1.0f;
            tables[2][i] = phase < 0.5f ? -1.0f : 1.0f;
            tables[3][i] = 1.0f - 4.0f * std::abs(phase - 0.5f);
        }

        // Anti-aliased copies for playback; built once, off the audio thread.
        for (std::size_t i = 0; i < tables.size(); ++i)
            bandLimited[i].build(tables[i]);
    }

    static const WavetableBank& builtIn() noexcept {
        static const WavetableBank bank;
        return bank;
    }

    const Table& getTable(Waveform waveform) const noexcept {
        switch (waveform){

            case Waveform::Sine:
                return tables[0];

            case Waveform::Sawtooth:
                return tables[1];

            case Waveform::Square:
                return tables[2];

            case Waveform::Triangle:
                return tables[3];

            case Waveform::Custom:
                return tables[1];
        }

        return tables[1];
    }

    // What the oscillators play: the same shapes, band-limited per pitch.
    const BandLimitedWavetable& getBandLimited(Waveform waveform) const noexcept {
        switch (waveform){
            case Waveform::Sine:     return bandLimited[0];
            case Waveform::Sawtooth: return bandLimited[1];
            case Waveform::Square:   return bandLimited[2];
            case Waveform::Triangle: return bandLimited[3];
            case Waveform::Custom:   return bandLimited[1];
        }
        return bandLimited[1];
    }

private:
    std::array<Table, 4> tables{};
    std::array<BandLimitedWavetable, 4> bandLimited{};

};
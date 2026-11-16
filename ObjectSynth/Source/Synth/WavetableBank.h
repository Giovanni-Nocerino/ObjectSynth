#pragma once

#include <array>
#include <cmath>
#include <cstddef>

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

    WavetableBank() noexcept {
        constexpr float twoPi = 6.283185307179586f;
        for (std::size_t i = 0; i < tableSize; ++i){

            const float phase = static_cast<float>(i)/static_cast<float>(tableSize);
            tables[0][i] = std::sin(twoPi * phase);
            tables[1][i] = 2.0f * phase - 1.0f;
            tables[2][i] = phase < 0.5f ? -1.0f : 1.0f;
            tables[3][i] = 1.0f - 4.0f * std::abs(phase - 0.5f);
        }
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

private:
    std::array<Table, 4> tables{};

};
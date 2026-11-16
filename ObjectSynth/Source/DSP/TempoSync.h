#pragma once

#include <array>
#include <cmath>

// Tempo-synced LFO rates: note divisions expressed in quarter-note beats.
namespace TempoSync
{
    struct Division
    {
        const char* name;
        double beats; // length of one LFO cycle, in quarter notes
    };

    // Ordered from slowest to fastest.
    inline constexpr std::array<Division, 15> divisions {{
        {"4 bars", 16.0},
        {"2 bars", 8.0},
        {"1/1",    4.0},
        {"1/2",    2.0},
        {"1/2T",   4.0 / 3.0},
        {"1/4.",   1.5},
        {"1/4",    1.0},
        {"1/4T",   2.0 / 3.0},
        {"1/8.",   0.75},
        {"1/8",    0.5},
        {"1/8T",   1.0 / 3.0},
        {"1/16.",  0.375},
        {"1/16",   0.25},
        {"1/16T",  1.0 / 6.0},
        {"1/32",   0.125},
    }};

    inline constexpr int defaultIndex = 6; // 1/4
    inline constexpr double fallbackBpm = 120.0;

    inline double beatsFor(int index) noexcept
    {
        if (index < 0) index = 0;
        if (index >= static_cast<int>(divisions.size()))
            index = static_cast<int>(divisions.size()) - 1;
        return divisions[static_cast<std::size_t>(index)].beats;
    }

    inline double rateHz(int index, double bpm) noexcept
    {
        if (!std::isfinite(bpm) || bpm <= 0.0)
            bpm = fallbackBpm;
        return (bpm / 60.0) / beatsFor(index);
    }

    // LFO phase (0..1) that keeps the cycle locked to the host's bar grid.
    inline double phaseAt(int index, double ppqPosition) noexcept
    {
        const double cycles = ppqPosition / beatsFor(index);
        return cycles - std::floor(cycles);
    }
}

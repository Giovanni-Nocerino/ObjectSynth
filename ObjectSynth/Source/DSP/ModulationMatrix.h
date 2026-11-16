#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Keep these indices synchronized with the APVTS choice options.
// Only continuous, real-time-safe destinations are exposed here.
enum class ModDestination : int
{
    None = 0,
    LPCutoff,
    HPCutoff,
    BPCutoff,
    LPResonance,
    HPResonance,
    BPResonance,
    LPMix,
    HPMix,
    BPMix,
    VoiceFilterCutoff,
    FilterEnvelopeAmount,
    DistortionDrive,
    DistortionMix,
    ChorusRate,
    ChorusDepth,
    ChorusDelay,
    ChorusMix,
    Count
};

inline constexpr std::array<const char*,
    static_cast<std::size_t>(ModDestination::Count)> modDestinationNames {
    "None", "LP Cutoff", "HP Cutoff", "BP Cutoff",
    "LP Resonance", "HP Resonance", "BP Resonance",
    "LP Mix", "HP Mix", "BP Mix",
    "Voice LP Cutoff", "Filter Env Amount",
    "Distortion Drive", "Distortion Mix",
    "Chorus Rate", "Chorus Depth", "Chorus Delay", "Chorus Mix"
};

struct ModulationRoute
{
    ModDestination destination = ModDestination::None;
    float depth = 0.0f;
};

class ModulationMatrix final
{
public:
    static constexpr std::size_t maxRoutes = 4;
    static constexpr std::size_t destinationCount =
        static_cast<std::size_t>(ModDestination::Count);

    using Values = std::array<float, destinationCount>;

    void setRoute(std::size_t slot, ModulationRoute route) noexcept
    {
        if (slot >= routes.size() || !std::isfinite(route.depth))
            return;

        const int index = static_cast<int>(route.destination);
        if (index < 0 || index >= static_cast<int>(destinationCount))
            return;

        route.depth = std::clamp(route.depth, -1.0f, 1.0f);
        routes[slot] = route;
    }

    Values evaluate(float sourceValue) const noexcept
    {
        Values result {};
        const float source = std::isfinite(sourceValue)
            ? std::clamp(sourceValue, -1.0f, 1.0f) : 0.0f;

        for (const auto& route : routes)
        {
            const auto index = static_cast<std::size_t>(route.destination);
            if (index > 0 && index < destinationCount)
                result[index] += route.depth * source;
        }

        for (auto& value : result)
            value = std::clamp(value, -1.0f, 1.0f);

        return result;
    }

private:
    std::array<ModulationRoute, maxRoutes> routes {};
};

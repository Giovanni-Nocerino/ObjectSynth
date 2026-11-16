
#pragma once

#include <cmath>

class ModulationSmoother final
{
public:
    bool prepare(double sampleRate,
                 int intervalSamples,
                 double timeSeconds = 0.003) noexcept
    {
        if (!std::isfinite(sampleRate) ||
            sampleRate <= 0.0 ||
            intervalSamples <= 0 ||
            !std::isfinite(timeSeconds) ||
            timeSeconds < 0.0)
        {
            feedback = 0.0;
            return false;
        }

        feedback = timeSeconds == 0.0
            ? 0.0
            : std::exp(
                -static_cast<double>(intervalSamples) /
                (sampleRate * timeSeconds)
            );

        return true;
    }

    void reset(float initialValue) noexcept
    {
        current = std::isfinite(initialValue)
            ? initialValue
            : 0.0;
    }

    float processTarget(float target) noexcept
    {
        if (std::isfinite(target))
        {
            current =
                feedback * current +
                (1.0 - feedback) * target;
        }

        return static_cast<float>(current);
    }

private:
    double feedback = 0.0;
    double current = 0.0;
};

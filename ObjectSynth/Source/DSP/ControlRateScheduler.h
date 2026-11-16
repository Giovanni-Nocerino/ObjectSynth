
#pragma once

#include <algorithm>
#include <cassert>

class ControlRateScheduler final
{
public:
    static constexpr int intervalSamples = 32;

    void reset() noexcept {
        samplesUntilUpdate = 0;
    }

    bool isUpdateDue() const noexcept {
        return samplesUntilUpdate == 0;
    }

    int nextChunkSize(int availableSamples) const noexcept{
        if (availableSamples <= 0)
            return 0;

        const int remaining = samplesUntilUpdate == 0
            ? intervalSamples
            : samplesUntilUpdate;

        return std::min(availableSamples, remaining);
    }

    void advance(int processedSamples) noexcept
    {
        const int remaining = samplesUntilUpdate == 0
            ? intervalSamples
            : samplesUntilUpdate;

        assert(processedSamples > 0 &&
               processedSamples <= remaining);

        if (processedSamples <= 0 ||
            processedSamples > remaining)
            return;

        samplesUntilUpdate = remaining - processedSamples;
    }

private:
    int samplesUntilUpdate = 0;
};


#include "../Source/DSP/LFO.h"

#include <cassert>
#include <cmath>
#include <limits>

int main()
{
    for (int sampleRate : {44100, 48000, 96000})
    {
        LFO lfo;

        assert(lfo.prepare(sampleRate));

        lfo.setRateHz(1.0);

        // Initial phase: sin(0) = 0.
        assert(std::abs(lfo.getNextSample()) < 1e-5f);

        // Advance to one quarter of a cycle.
        for (int i = 1; i < sampleRate / 4; ++i)
            lfo.getNextSample();

        assert(
            std::abs(lfo.getNextSample() - 1.0f) < 1e-4f
        );

        // Complete the cycle.
        for (int i = sampleRate / 4 + 1;
             i < sampleRate; ++i)
        {
            lfo.getNextSample();
        }

        assert(std::abs(lfo.getNextSample()) < 1e-4f);

        // An invalid rate must not corrupt the LFO.
        lfo.setRateHz(
            std::numeric_limits<double>::quiet_NaN()
        );

        assert(std::isfinite(lfo.getNextSample()));
    }

    LFO invalidLFO;

    assert(!invalidLFO.prepare(0.0));
    assert(invalidLFO.getNextSample() == 0.0f);

    return 0;
}

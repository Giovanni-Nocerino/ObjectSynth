
#include "../Source/DSP/ModulationSmoother.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main()
{
    ModulationSmoother smoother;

    assert(smoother.prepare(48000.0, 32, 0.003));

    const float initial = std::log2(800.0f);
    const float target = std::log2(3200.0f);

    smoother.reset(initial);

    float previous = initial;

    for (int i = 0; i < 50; ++i)
    {
        const float current =
            smoother.processTarget(target);

        assert(std::isfinite(current));
        assert(current >= previous);
        assert(current <= target);

        previous = current;
    }

    assert(std::abs(previous - target) < 0.001f);

    // Invalid input must not corrupt the state.
    const float result = smoother.processTarget(
        std::numeric_limits<float>::quiet_NaN()
    );

    assert(std::isfinite(result));

    std::cout << "Smoother tests: PASSED\n";
}

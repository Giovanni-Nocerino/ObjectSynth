
#include "../Source/DSP/ModulationMatrix.h"

#include <cassert>
#include <cmath>
#include <limits>

int main()
{
    ModulationMatrix matrix;

    matrix.setRoute(
        0, {ModDestination::LPCutoff, 0.5f}
    );

    matrix.setRoute(
        1, {ModDestination::BPCutoff, -0.25f}
    );

    const auto lp =
        static_cast<std::size_t>(ModDestination::LPCutoff);

    const auto hp =
        static_cast<std::size_t>(ModDestination::HPCutoff);

    const auto bp =
        static_cast<std::size_t>(ModDestination::BPCutoff);

    const auto positive = matrix.evaluate(1.0f);

    assert(std::abs(positive[lp] - 0.5f) < 1e-6f);
    assert(positive[hp] == 0.0f);
    assert(std::abs(positive[bp] + 0.25f) < 1e-6f);

    const auto negative = matrix.evaluate(-1.0f);

    assert(std::abs(negative[lp] + 0.5f) < 1e-6f);
    assert(std::abs(negative[bp] - 0.25f) < 1e-6f);

    // Multiple routes on the same destination are bounded.
    matrix.setRoute(
        2, {ModDestination::LPCutoff, 0.75f}
    );

    assert(matrix.evaluate(1.0f)[lp] == 1.0f);

    // Invalid input must not corrupt the result.
    const auto invalid = matrix.evaluate(
        std::numeric_limits<float>::quiet_NaN()
    );

    for (const float value : invalid)
        assert(std::isfinite(value) && value == 0.0f);
}

#include "../Source/Synth/WavetableFrameExtractor.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
#include <iostream>

int main()
{
    constexpr int size = static_cast<int>(WavetableBank::tableSize);
    constexpr double pi = 3.14159265358979323846;
    std::vector<float> samples(32 * size);

    for (int frame = 0; frame < 32; ++frame)
        for (int i = 0; i < size; ++i)
            samples[static_cast<std::size_t>(frame * size + i)] =
                static_cast<float>(std::sin(2.0 * pi * (frame + 1) * i / size));

    const auto multi = WavetableFrameExtractor::extract(samples.data(), samples.size());
    assert(multi.status == WavetableProcessor::Status::Ok);
    assert(multi.frames.size() == 32);
    assert(multi.seamWarnings.size() == 32);
    assert(std::abs(multi.frames[0][size / 4] - 1.0f) < 0.01f);
    assert(std::abs(multi.frames[1][size / 4]) < 0.01f);

    const auto single = WavetableFrameExtractor::extract(samples.data(), size);
    assert(single.status == WavetableProcessor::Status::Ok);
    assert(single.frames.size() == 1);

    const auto invalid = WavetableFrameExtractor::extract(nullptr, size);
    assert(invalid.status == WavetableProcessor::Status::InvalidLength);

    std::vector<float> silent(size, 0.0f);
    const auto silence = WavetableFrameExtractor::extract(silent.data(), silent.size());
    assert(silence.status == WavetableProcessor::Status::Silence);

    std::vector<float> withNan(size, 0.0f);
    withNan[0] = std::numeric_limits<float>::quiet_NaN();
    const auto nan = WavetableFrameExtractor::extract(withNan.data(), withNan.size());
    assert(nan.status == WavetableProcessor::Status::InvalidSamples);

    std::cout << "MultiframeStandaloneTests PASSED\n";
}

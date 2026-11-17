#include "../Source/Synth/WavetableProcessor.h"
#include "../Source/Synth/WavetableOscillator.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
#include <iostream>

int main()
{
    constexpr std::size_t N = 512;
    constexpr double pi = 3.14159265358979323846;
    std::vector<float> sine(N);
    for (std::size_t i = 0; i < N; ++i)
        sine[i] = 0.5f + static_cast<float>(0.7 * std::sin(2.0 * pi * i / N));

    WavetableBank::Table table;
    assert(WavetableProcessor::fromSingleCycle(sine.data(), sine.size(), table)
           == WavetableProcessor::Status::Ok);
    float peak = 0.0f;
    double mean = 0.0;
    for (float sample : table)
    {
        peak = std::max(peak, std::abs(sample));
        mean += sample;
    }
    mean /= table.size();
    assert(std::abs(mean) < 0.00001);
    assert(std::abs(peak - 1.0f) < 0.00001f);
    assert(std::abs(table[0]) < 0.001f);
    assert(table[table.size() / 4] > 0.99f);

    std::vector<float> silence(256, 0.5f);
    assert(WavetableProcessor::fromSingleCycle(silence.data(), silence.size(), table)
           == WavetableProcessor::Status::Silence);
    silence[3] = std::numeric_limits<float>::quiet_NaN();
    assert(WavetableProcessor::fromSingleCycle(silence.data(), silence.size(), table)
           == WavetableProcessor::Status::InvalidSamples);
    assert(WavetableProcessor::fromSingleCycle(nullptr, N, table)
           == WavetableProcessor::Status::InvalidLength);

    // Snapshot pointers may be shared by many oscillators without mutating the table.
    WavetableOscillator left, right;
    left.prepare(48000.0);
    right.prepare(48000.0);
    left.setCustomTable(&WavetableBank::builtIn().getBandLimited(Waveform::Sine));
    right.setCustomTable(&WavetableBank::builtIn().getBandLimited(Waveform::Sine));
    left.setWaveform(Waveform::Custom);
    right.setWaveform(Waveform::Custom);
    left.setFrequency(220.0);
    right.setFrequency(220.0);
    for (int i = 0; i < 100; ++i)
        assert(std::abs(left.getNextSample() - right.getNextSample()) < 1e-6f);

    std::cout << "WavetableProcessorTests: passed\n";
}

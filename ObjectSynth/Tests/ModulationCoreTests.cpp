#include "../Source/DSP/LFO.h"
#include "../Source/DSP/ControlRateScheduler.h"
#include "../Source/DSP/ModulationMatrix.h"
#include "../Source/DSP/ModulationSmoother.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

static void testLfo()
{
    for (const int sampleRate : {44100, 48000, 96000})
    {
        LFO lfo;
        assert(lfo.prepare(sampleRate));
        lfo.setRateHz(1.0);
        assert(std::abs(lfo.getNextSample()) < 1.0e-5f);
        for (int i = 1; i < sampleRate / 4; ++i) lfo.getNextSample();
        assert(std::abs(lfo.getNextSample() - 1.0f) < 1.0e-4f);
        for (int i = sampleRate / 4 + 1; i < sampleRate; ++i) lfo.getNextSample();
        assert(std::abs(lfo.getNextSample()) < 1.0e-4f);

        LFO stepped;
        LFO skipped;
        assert(stepped.prepare(sampleRate));
        assert(skipped.prepare(sampleRate));
        stepped.setRateHz(17.5);
        skipped.setRateHz(17.5);
        for (int i = 0; i < sampleRate; ++i) stepped.getNextSample();
        skipped.advance(sampleRate);
        assert(std::abs(stepped.getPhase() - skipped.getPhase()) < 1.0e-7);
    }

    LFO invalid;
    assert(!invalid.prepare(0.0));
    assert(invalid.getNextSample() == 0.0f);
    assert(invalid.prepare(48000.0));
    invalid.setRateHz(std::numeric_limits<double>::quiet_NaN());
    assert(std::isfinite(invalid.getNextSample()));
    invalid.reset(std::numeric_limits<double>::infinity());
    assert(invalid.getPhase() == 0.0);
}

static std::vector<int> simulateSchedule(const std::vector<int>& blocks)
{
    ControlRateScheduler scheduler;
    int absolute = 0;
    std::vector<int> events;
    for (const int block : blocks)
    {
        int offset = 0;
        while (offset < block)
        {
            if (scheduler.isUpdateDue()) events.push_back(absolute + offset);
            const int chunk = scheduler.nextChunkSize(block - offset);
            assert(chunk > 0 && chunk <= 32);
            scheduler.advance(chunk);
            offset += chunk;
        }
        absolute += block;
    }
    return events;
}

static void testScheduler()
{
    std::vector<int> expected;
    for (int sample = 0; sample < 520; sample += 32) expected.push_back(sample);
    assert(simulateSchedule({520}) == expected);
    assert(simulateSchedule({128, 256, 136}) == expected);
    assert(simulateSchedule({40, 480}) == expected);
    assert(simulateSchedule({1, 7, 63, 125, 324}) == expected);
}

static void testMatrix()
{
    static_assert(modDestinationNames.size() == ModulationMatrix::destinationCount);
    static_assert(ModulationMatrix::maxRoutes == 4);
    ModulationMatrix matrix;
    const auto lp = static_cast<std::size_t>(ModDestination::LPCutoff);
    const auto bp = static_cast<std::size_t>(ModDestination::BPCutoff);
    const auto drive = static_cast<std::size_t>(ModDestination::DistortionDrive);
    const auto voice = static_cast<std::size_t>(ModDestination::VoiceFilterCutoff);
    matrix.setRoute(0, {ModDestination::LPCutoff, 0.5f});
    matrix.setRoute(1, {ModDestination::BPCutoff, -0.25f});
    matrix.setRoute(2, {ModDestination::DistortionDrive, 1.0f});
    matrix.setRoute(3, {ModDestination::VoiceFilterCutoff, -0.9f});
    auto values = matrix.evaluate(1.0f);
    assert(std::abs(values[lp] - 0.5f) < 1.0e-6f);
    assert(std::abs(values[bp] + 0.25f) < 1.0e-6f);
    assert(values[drive] == 1.0f);
    assert(std::abs(values[voice] + 0.9f) < 1.0e-6f);
    values = matrix.evaluate(-1.0f);
    assert(std::abs(values[lp] + 0.5f) < 1.0e-6f);
    matrix.setRoute(2, {ModDestination::LPCutoff, 0.9f});
    assert(matrix.evaluate(1.0f)[lp] == 1.0f);
    matrix.setRoute(2, {static_cast<ModDestination>(-123), 0.5f});
    assert(matrix.evaluate(1.0f)[lp] == 1.0f);
    matrix.setRoute(0, {ModDestination::None, 0.0f});
    assert(std::abs(matrix.evaluate(1.0f)[lp] - 0.9f) < 1.0e-6f);
    auto nanResults = matrix.evaluate(std::numeric_limits<float>::quiet_NaN());
    for (auto v : nanResults) assert(std::isfinite(v) && v == 0.0f);
}

static void testSmoother()
{
    ModulationSmoother smooth;
    assert(smooth.prepare(48000, 32, 0.003));
    const float initial = std::log2(800.0f);
    const float target = std::log2(3200.0f);
    smooth.reset(initial);
    float last = initial;
    for (int i = 0; i < 60; ++i)
    {
        const float next = smooth.processTarget(target);
        assert(std::isfinite(next) && next >= last && next <= target);
        last = next;
    }
    assert(std::abs(last - target) < 0.001f);
    assert(std::isfinite(smooth.processTarget(std::numeric_limits<float>::quiet_NaN())));
}

int main()
{
    testLfo();
    testScheduler();
    testMatrix();
    testSmoother();
    std::cout << "Modulation Core Tests: PASSED\n";
}

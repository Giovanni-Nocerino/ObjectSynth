
#include "Synth/WavetableOscillator.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    WavetableOscillator osc;

    osc.prepare(48000.0);
    osc.setFrequency(375.0);
    osc.setWaveform(Waveform::Sine);

    const float first = osc.getNextSample();

    for (int i = 1; i < 128; ++i)
        osc.getNextSample();

    const float afterPeriod = osc.getNextSample();

    assert(std::abs(first - afterPeriod) < 1e-5f);

    std::cout << "Periodicity test passed!\n";


    
    WavetableOscillator quarterOsc;

    quarterOsc.prepare(48000.0);
    quarterOsc.setFrequency(375.0);
    quarterOsc.setWaveform(Waveform::Sine);

    for (int i = 0; i < 32; ++i)
        quarterOsc.getNextSample();

    const float quarterSample = quarterOsc.getNextSample();

    assert(std::abs(quarterSample - 1.0f) < 1e-5f);

    std::cout << "Quarter-period test passed!\n";



    // Linear interpolation on the full-band table. The oscillator reads a
    // band-limited copy above ~21 Hz at 48 kHz (BandLimitedWavetable), so the
    // test uses a frequency low enough to play level 0, the raw saw table.
    WavetableOscillator interpolationOsc;

    interpolationOsc.prepare(48000.0);
    interpolationOsc.setFrequency(1.5);
    interpolationOsc.setWaveform(Waveform::Sawtooth);

    const float sample0 = interpolationOsc.getNextSample();
    const float sample1 = interpolationOsc.getNextSample();

    // Step = 1.5 * 2048 / 48000 = 0.064 table samples; saw slope = 2 / 2048.
    const float expected1 = -1.0f + 0.064f * (2.0f / 2048.0f);
    assert(std::abs(sample0 - (-1.0f)) < 1e-6f);
    assert(std::abs(sample1 - expected1) < 1e-6f);

    std::cout << "Interpolation test passed!\n";


}

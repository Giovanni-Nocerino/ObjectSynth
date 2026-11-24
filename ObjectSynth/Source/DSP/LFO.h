#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

class LFO final {
public:
    enum class Waveform {
        Sine, Saw, Square, Triangle,
        Custom // one cycle of the oscillator's custom wavetable frame
    };

    bool prepare(double newSampleRate) noexcept {

        if (!std::isfinite(newSampleRate) || newSampleRate <= 2.0*maxRateHz){
            prepared = false;
            return false;
        }

        sampleRate = newSampleRate;
        phaseIncrement = rateHz / sampleRate;
        phase = 0.0;
        prepared = true;

        return true;
    }

    void setRateHz(double newRateHz) noexcept {

        if (!std::isfinite(newRateHz)) return;

        rateHz = std::clamp(newRateHz, minRateHz, maxRateHz);

        phaseIncrement = rateHz/sampleRate;
    }

    void setWaveform (Waveform newWaveform) noexcept {
        switch (newWaveform){
            case Waveform::Sine:
            case Waveform::Saw:
            case Waveform::Square:
            case Waveform::Triangle:
            case Waveform::Custom:
                waveform = newWaveform;
                break;
            default:
                break;
        }
    }

    // Table read by Waveform::Custom (one cycle, values in -1..1). Set once per
    // block by the owner, which keeps it alive for that block; nullptr makes
    // Custom fall back to a sine.
    void setCustomTable(const float* samples, std::size_t size) noexcept {
        customTable = size > 1 ? samples : nullptr;
        customTableSize = customTable != nullptr ? size : 0;
    }

    void reset(double newPhase = 0.0) noexcept {
        phase = std::isfinite(newPhase) ? newPhase -std::floor(newPhase) : 0.0;
    }

    // Advance without evaluating intermediate wave samples. The phase
    // advances in audio samples, not in host buffer or control ticks.
    void advance(int numSamples) noexcept
    {
        if (!prepared || numSamples <= 0)
            return;

        phase += phaseIncrement * static_cast<double>(numSamples);
        phase -= std::floor(phase);
    }

    double getPhase() const noexcept { return phase; }

    float getNextSample() noexcept {
        
        if (!prepared) return 0.0f;

        double value = 0.0;
        switch (waveform)
        {
        case Waveform::Sine:
            value = std::sin(twoPi * phase);
            break;
        
        case Waveform::Saw:
            value = 2.0 * phase -1.0;
            break;
        
        case Waveform::Square:
            value = phase < 0.5 ? 1.0 : -1.0;
            break;

        case Waveform::Triangle:
            value = 1.0 - 4.0 * std::abs(phase - 0.5);
            break;

        case Waveform::Custom:
            value = customTable != nullptr ? readCustom(phase) : std::sin(twoPi * phase);
            break;
        }
        
        phase += phaseIncrement;

        if (phase >= 1.0) phase -= 1.0;

        return static_cast<float>(value);
    }


private:
    // Linear interpolation, wrapping at the end of the cycle.
    double readCustom(double atPhase) const noexcept {
        const double position = atPhase * static_cast<double>(customTableSize);
        const auto i = static_cast<std::size_t>(position) % customTableSize;
        const auto next = (i + 1) % customTableSize;
        const double fraction = position - std::floor(position);
        return customTable[i] + (customTable[next] - customTable[i]) * fraction;
    }
    
    static constexpr double twoPi = 6.28318530717958647692;
    // Wide enough for tempo-synced rates (4 bars at slow tempos up to 1/32 at fast ones).
    static constexpr double minRateHz = 0.01;
    static constexpr double maxRateHz = 50.0;

    double sampleRate = 44100.0;
    double rateHz = 1.0;

    double phase = 0.0;
    double phaseIncrement = 0.0;

    Waveform waveform = Waveform::Sine;
    const float* customTable = nullptr;
    std::size_t customTableSize = 0;
    bool prepared = false;

};
#pragma once

#include <algorithm>
#include <cmath>

class LFO final {
public:
    enum class Waveform {
        Sine, Saw, Square, Triangle
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
                waveform = newWaveform;
                break;
            default:
                break;
        }
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
        }
        
        phase += phaseIncrement;

        if (phase >= 1.0) phase -= 1.0;

        return static_cast<float>(value);
    }


private:
    
    static constexpr double twoPi = 6.28318530717958647692;
    static constexpr double minRateHz = 0.05;
    static constexpr double maxRateHz = 20.0;

    double sampleRate = 44100.0;
    double rateHz = 1.0;

    double phase = 0.0;
    double phaseIncrement = 0.0;

    Waveform waveform = Waveform::Sine;
    bool prepared = false;

};
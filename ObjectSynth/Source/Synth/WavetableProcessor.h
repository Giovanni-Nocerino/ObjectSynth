#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "WavetableBank.h"

class WavetableProcessor final {
public:

    static constexpr std::size_t minInputSamples = 16;
    static constexpr std::size_t maxInputSamples = 65536;

    enum class Status {
        Ok, InvalidLength, InvalidSamples, Silence
    };

    static Status fromSingleCycle(const float* source, std::size_t length, WavetableBank::Table& destination) noexcept {
        destination.fill(0.0f);

        if (source == nullptr ||
            length < minInputSamples ||
            length > maxInputSamples)
            return Status::InvalidLength;
        
        for (std::size_t i = 0; i < length; ++i) {
            if (!std::isfinite(source[i])) return Status::InvalidSamples;
        }

        const auto tableSize = destination.size();
        double mean = 0.0;

        for (std::size_t i = 0; i < tableSize; ++i){
            const double position = static_cast<double>(i) * static_cast<double>(length) / static_cast<double>(tableSize);
            
            const auto first = static_cast<std::size_t>(position); 
            const auto second = static_cast<std::size_t>(first +1) % length;

            const double alpha = position - static_cast<double>(first);
            const double sample = (1 - alpha) * source[first] + alpha * source[second];
            destination[i] = static_cast<float>(sample);
            mean += sample;
        }

        mean /= static_cast<double>(tableSize);
        float peak = 0.0f;
        for (auto& sample : destination){
            sample -= static_cast<float>(mean);
            peak = std::max(peak, std::abs(sample));
        }

        if (peak < 1.0e-6f){
            destination.fill(0.0f);
            return Status::Silence;
        };

        for (auto& sample : destination) {
            sample /= peak;
        }

        return Status::Ok;
    }

};
#pragma once

#include "WavetableProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>
#include <utility>

// Offline-only helper. Audio thread never calls this class.
class WavetableFrameExtractor final
{
public:
    static constexpr std::size_t maxFrames = 32;

    struct Result
    {
        std::vector<WavetableBank::Table> frames;
        std::vector<bool> seamWarnings;
        WavetableProcessor::Status status = WavetableProcessor::Status::InvalidLength;
        std::size_t failedFrame = 0;
    };

    static Result extract(const float* samples, std::size_t totalSamples)
    {
        Result result;

        if (samples == nullptr ||
            totalSamples < WavetableProcessor::minInputSamples ||
            totalSamples > WavetableProcessor::maxInputSamples)
            return result;

        // Serum-style multiframe files consist of consecutive 2048-sample cycles.
        // This is a heuristic: arbitrary single-cycle WAVs of length 4096, 8192,
        // etc. will also be interpreted as multiple frames.
        const bool multiFrame =
            totalSamples > WavetableBank::tableSize &&
            totalSamples % WavetableBank::tableSize == 0;

        const std::size_t count = multiFrame
            ? totalSamples / WavetableBank::tableSize
            : 1;

        if (count == 0 || count > maxFrames)
            return result;

        const std::size_t inputFrameSize = totalSamples / count;
        result.frames.reserve(count);
        result.seamWarnings.reserve(count);

        for (std::size_t frame = 0; frame < count; ++frame)
        {
            const float* input = samples + frame * inputFrameSize;
            WavetableBank::Table table{};

            const auto status = WavetableProcessor::fromSingleCycle(
                input, inputFrameSize, table);

            if (status != WavetableProcessor::Status::Ok)
            {
                result.frames.clear();
                result.seamWarnings.clear();
                result.status = status;
                result.failedFrame = frame;
                return result;
            }

            const auto bounds = std::minmax_element(input, input + inputFrameSize);
            const float range = *bounds.second - *bounds.first;
            const float seamJump = std::abs(input[0] - input[inputFrameSize - 1]);
            const float firstSlope = std::abs(input[1] - input[0]);
            const float lastSlope = std::abs(input[inputFrameSize - 1] - input[inputFrameSize - 2]);
            const float tolerance = std::max(
                4.0f * std::max(firstSlope, lastSlope), 0.05f * range);

            result.frames.push_back(std::move(table));
            result.seamWarnings.push_back(seamJump > tolerance);
        }

        result.status = WavetableProcessor::Status::Ok;
        return result;
    }
};

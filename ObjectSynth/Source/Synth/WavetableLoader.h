#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "WavetableBank.h"

#include <vector>

class WavetableLoader final
{
public:
    struct Result
    {
        // Kept for compatibility with existing single-cycle smoke tests.
        WavetableBank::Table table{};
        std::vector<WavetableBank::Table> frames;
        std::vector<bool> seamWarnings;
        juce::String error;
        bool success = false;
        bool seamWarning = false;
    };

    // Historical method name retained; it now recognises multiframe WAVs.
    static Result loadSingleCycleWav(const juce::File& file);
};

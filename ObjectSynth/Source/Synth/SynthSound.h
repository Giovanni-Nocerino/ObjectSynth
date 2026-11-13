
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

class SynthSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override
    {
        return true;
    }

    bool appliesToChannel(int) override
    {
        return true;
    }
};

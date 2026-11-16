
#include "../Source/Synth/WavetableLoader.h"

#include <cmath>
#include <algorithm>
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: ObjectSynthWavetableTest file.wav\n";
        return 1;
    }

    const juce::File file(
        juce::String::fromUTF8(argv[1])
    );

    const auto result =
        WavetableLoader::loadSingleCycleWav(file);

    if (!result.success)
    {
        std::cerr
            << result.error.toStdString() << '\n';

        return 1;
    }

    if (result.frames.empty() ||
        result.seamWarnings.size() != result.frames.size())
    {
        std::cerr << "Invalid frame collection\n";
        return 1;
    }

    std::cout << "Frames detected: " << result.frames.size() << '\n';
    int warningCount = 0;
    for (bool warning : result.seamWarnings)
        warningCount += warning ? 1 : 0;
    std::cout << "Seam warnings: " << warningCount << '\n';

    double mean = 0.0;
    float peak = 0.0f;

    for (const float sample : result.table)
    {
        mean += sample;
        peak = std::max(peak, std::abs(sample));
    }

    mean /= static_cast<double>(result.table.size());

    std::cout
        << "Wavetable loaded successfully\n"
        << "Samples: " << result.table.size() << '\n'
        << "Mean: " << mean << '\n'
        << "Peak: " << peak << '\n';

    return 0;
}

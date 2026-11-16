#include "WavetableLoader.h"
#include "WavetableFrameExtractor.h"

#include <cmath>
#include <memory>
#include <vector>
#include <utility>

WavetableLoader::Result
WavetableLoader::loadSingleCycleWav(const juce::File& file)
{
    Result result;

    if (!file.existsAsFile())
    {
        result.error = "File not found";
        return result;
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerFormat(new juce::WavAudioFormat(), true);

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(file));

    if (reader == nullptr)
    {
        result.error = "Invalid or unsupported WAV file";
        return result;
    }

    if (reader->numChannels < 1 || reader->numChannels > 2)
    {
        result.error = "Only mono and stereo WAV files are supported";
        return result;
    }

    if (!std::isfinite(reader->sampleRate) || reader->sampleRate <= 0.0)
    {
        result.error = "Invalid WAV sample rate";
        return result;
    }

    const auto numFrames = reader->lengthInSamples;
    if (numFrames < static_cast<juce::int64>(WavetableProcessor::minInputSamples) ||
        numFrames > static_cast<juce::int64>(WavetableProcessor::maxInputSamples))
    {
        result.error = "Invalid wavetable length";
        return result;
    }

    const int channels = static_cast<int>(reader->numChannels);
    const int frames = static_cast<int>(numFrames);

    juce::AudioBuffer<float> audio(channels, frames);
    if (!reader->read(&audio, 0, frames, 0, true, true))
    {
        result.error = "Cannot read audio samples";
        return result;
    }

    std::vector<float> mono(static_cast<std::size_t>(frames));
    const float* left = audio.getReadPointer(0);
    const float* right = channels == 2 ? audio.getReadPointer(1) : nullptr;

    for (int i = 0; i < frames; ++i)
        mono[static_cast<std::size_t>(i)] = right == nullptr
            ? left[i] : 0.5f * (left[i] + right[i]);

    auto extracted = WavetableFrameExtractor::extract(mono.data(), mono.size());

    if (extracted.status != WavetableProcessor::Status::Ok)
    {
        const juce::String frameInfo =
            " (frame " + juce::String(static_cast<int>(extracted.failedFrame + 1)) + ")";
        switch (extracted.status)
        {
            case WavetableProcessor::Status::InvalidLength:
                result.error = "Invalid wavetable length";
                break;
            case WavetableProcessor::Status::InvalidSamples:
                result.error = "WAV contains invalid samples" + frameInfo;
                break;
            case WavetableProcessor::Status::Silence:
                result.error = "WAV frame contains no usable waveform" + frameInfo;
                break;
            case WavetableProcessor::Status::Ok:
                break;
        }
        return result;
    }

    result.frames = std::move(extracted.frames);
    result.seamWarnings = std::move(extracted.seamWarnings);
    result.table = result.frames.front();
    result.seamWarning = result.seamWarnings.front();
    result.success = true;
    return result;
}

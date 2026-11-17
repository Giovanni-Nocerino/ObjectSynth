#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

// Anti-aliasing for wavetable playback ("mipmapped" wavetables).
//
// Problem: a 2048-sample table holds harmonics up to the 1023rd. Played at
// frequency f, harmonic k sounds at k * f; every harmonic above Nyquist
// (sampleRate / 2) folds back into the audible band at a frequency that is
// NOT a multiple of f. Those inharmonic tones are what makes high notes
// sound distorted (e.g. a naive saw at C5 had an alias only 27 dB down).
//
// Fix: keep several copies of the same cycle, each with the harmonics above
// a limit removed (via FFT), and let each oscillator read the copy whose
// highest harmonic stays below maxPartial * sampleRate for its frequency.
// Copies are spaced by half an octave, so a note keeps at least ~70% of the
// partials that could legally be heard.
//
// Building happens off the audio thread (built-in tables at start-up,
// custom WAV frames when they are loaded). forFrequency() is real-time safe.
// No JUCE dependency, so it is usable from the standalone tests.
class BandLimitedWavetable final
{
public:
    static constexpr std::size_t tableSize = 2048;
    using Table = std::array<float, tableSize>;

    // Highest harmonic kept by each level, from full bandwidth down to the
    // bare fundamental, in half-octave steps (1024 / sqrt(2)^n).
    static constexpr std::array<int, 19> harmonicLimits {
        1024, 724, 512, 362, 256, 181, 128, 90, 64, 45, 32, 22, 16, 11, 8, 5, 4, 2, 1
    };
    static constexpr std::size_t levelCount = harmonicLimits.size();

    // Partials are kept below this fraction of the sample rate (0.5 = Nyquist);
    // the margin keeps the top partials clear of the folding point.
    static constexpr double maxPartial = 0.45;

    BandLimitedWavetable() = default;
    explicit BandLimitedWavetable(const Table& source) { build(source); }

    // Level 0 is the source table unchanged; every other level is the source
    // with all harmonics above harmonicLimits[level] set to zero.
    void build(const Table& source)
    {
        levels[0] = source;

        std::vector<double> re(tableSize), im(tableSize);
        for (std::size_t i = 0; i < tableSize; ++i)
        {
            re[i] = static_cast<double>(source[i]);
            im[i] = 0.0;
        }
        fft(re, im, false);

        std::vector<double> levelRe(tableSize), levelIm(tableSize);
        for (std::size_t level = 1; level < levelCount; ++level)
        {
            const auto limit = static_cast<std::size_t>(harmonicLimits[level]);
            for (std::size_t bin = 0; bin < tableSize; ++bin)
            {
                // Bin k and its mirror N - k hold harmonic k.
                const std::size_t harmonic = bin <= tableSize / 2 ? bin : tableSize - bin;
                const bool keep = harmonic <= limit;
                levelRe[bin] = keep ? re[bin] : 0.0;
                levelIm[bin] = keep ? im[bin] : 0.0;
            }
            fft(levelRe, levelIm, true);
            for (std::size_t i = 0; i < tableSize; ++i)
                levels[level][i] = static_cast<float>(levelRe[i]);
        }
    }

    // Level to read for an oscillator advancing `cyclesPerSample`
    // (= frequency / sampleRate) cycles per output sample.
    static std::size_t levelFor(double cyclesPerSample) noexcept
    {
        if (!(cyclesPerSample > 0.0))
            return 0;
        const double highestAllowedHarmonic = maxPartial / cyclesPerSample;
        for (std::size_t level = 0; level < levelCount; ++level)
            if (static_cast<double>(harmonicLimits[level]) <= highestAllowedHarmonic)
                return level;
        return levelCount - 1;
    }

    const Table& forFrequency(double cyclesPerSample) const noexcept
    {
        return levels[levelFor(cyclesPerSample)];
    }

    const Table& level(std::size_t index) const noexcept
    {
        return levels[index < levelCount ? index : levelCount - 1];
    }

private:
    // In-place iterative radix-2 FFT on separate real/imaginary arrays.
    // inverse = true also divides by N. Plain doubles keep it fast in Debug.
    static void fft(std::vector<double>& re, std::vector<double>& im, bool inverse)
    {
        const std::size_t n = re.size();

        for (std::size_t i = 1, j = 0; i < n; ++i)
        {
            std::size_t bit = n >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
            {
                std::swap(re[i], re[j]);
                std::swap(im[i], im[j]);
            }
        }

        const double sign = inverse ? 1.0 : -1.0;
        for (std::size_t length = 2; length <= n; length <<= 1)
        {
            const double angle = sign * 2.0 * 3.14159265358979323846 / static_cast<double>(length);
            const double stepRe = std::cos(angle), stepIm = std::sin(angle);
            for (std::size_t start = 0; start < n; start += length)
            {
                double wRe = 1.0, wIm = 0.0;
                for (std::size_t k = 0; k < length / 2; ++k)
                {
                    const std::size_t a = start + k, b = a + length / 2;
                    const double tRe = re[b] * wRe - im[b] * wIm;
                    const double tIm = re[b] * wIm + im[b] * wRe;
                    re[b] = re[a] - tRe;
                    im[b] = im[a] - tIm;
                    re[a] += tRe;
                    im[a] += tIm;
                    const double nextRe = wRe * stepRe - wIm * stepIm;
                    wIm = wRe * stepIm + wIm * stepRe;
                    wRe = nextRe;
                }
            }
        }

        if (inverse)
            for (std::size_t i = 0; i < n; ++i)
            {
                re[i] /= static_cast<double>(n);
                im[i] /= static_cast<double>(n);
            }
    }

    std::array<Table, levelCount> levels {};
};

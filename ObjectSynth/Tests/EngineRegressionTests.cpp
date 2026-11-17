// Engine regression tests: drive the real ObjectSynthAudioProcessor offline,
// the same way a host does (processBlock with MIDI), and check the output.
//
//   cmake --build --preset debug --target ObjectSynthEngineTests
//   ctest --preset debug
#include "../Source/PluginProcessor.h"
#include "../Source/Synth/LiveADSR.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace
{
    int failures = 0;

    void check(bool ok, const juce::String& what)
    {
        std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
        if (! ok)
            ++failures;
    }

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    // Events are placed after this many samples so start-up parameter
    // smoothing (bypass fades) has settled before anything is measured.
    constexpr int warmUp = 10 * blockSize;

    using Setup = std::function<void(ObjectSynthAudioProcessor&)>;

    void set(ObjectSynthAudioProcessor& p, const char* id, float value)
    {
        auto* parameter = p.getAPVTS().getParameter(id);
        jassert(parameter != nullptr);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }

    // Every module that could make the signal path non-linear is off.
    void cleanPatch(ObjectSynthAudioProcessor& p)
    {
        for (auto* id : {"filterBankEnabled", "filterEnvEnabled", "distortionEnabled",
                         "chorusEnabled", "lfoEnabled", "unisonEnabled"})
            set(p, id, 0.0f);
        p.setWaveform(Waveform::Sine);
        set(p, "attack", 0.005f);
        set(p, "release", 3.0f);
        // Level checks are calibrated at unity output gain; the default is -6 dB headroom.
        set(p, "outputVolume", 0.0f);
    }

    std::unique_ptr<ObjectSynthAudioProcessor> makeSynth(const Setup& setup, int numChannels = 2)
    {
        auto synth = std::make_unique<ObjectSynthAudioProcessor>();
        if (numChannels == 1)
        {
            auto layout = synth->getBusesLayout();
            layout.getChannelSet(false, 0) = juce::AudioChannelSet::mono();
            const bool accepted = synth->setBusesLayout(layout);
            jassert(accepted);
            juce::ignoreUnused(accepted);
        }
        if (setup)
            setup(*synth);
        synth->setRateAndBufferSizeDetails(sampleRate, blockSize);
        synth->prepareToPlay(sampleRate, blockSize);
        return synth;
    }

    struct Note
    {
        int sample;
        int number;
        bool on = true;
        float velocity = 1.0f;
    };

    struct Stereo
    {
        std::vector<float> left, right;
    };

    // Something done to the synth while it plays (like a user turning a knob):
    // applied just before the host block that contains `sample`.
    struct Action
    {
        int sample;
        std::function<void(ObjectSynthAudioProcessor&)> apply;
    };

    Stereo render(ObjectSynthAudioProcessor& p, int totalSamples, const std::vector<Note>& notes,
                  const std::vector<Action>& actions = {})
    {
        const int channels = p.getTotalNumOutputChannels();
        Stereo out;
        juce::AudioBuffer<float> buffer(channels, blockSize);
        for (int start = 0; start < totalSamples; start += blockSize)
        {
            for (const auto& action : actions)
                if (action.sample >= start && action.sample < start + blockSize)
                    action.apply(p);

            juce::MidiBuffer midi;
            for (const auto& n : notes)
                if (n.sample >= start && n.sample < start + blockSize)
                    midi.addEvent(n.on ? juce::MidiMessage::noteOn(1, n.number, n.velocity)
                                       : juce::MidiMessage::noteOff(1, n.number),
                                  n.sample - start);
            p.processBlock(buffer, midi);
            for (int i = 0; i < blockSize; ++i)
            {
                out.left.push_back(buffer.getSample(0, i));
                out.right.push_back(buffer.getSample(channels > 1 ? 1 : 0, i));
            }
        }
        return out;
    }

    float peak(const std::vector<float>& x, std::size_t from = 0, std::size_t to = SIZE_MAX)
    {
        float result = 0.0f;
        for (std::size_t i = from; i < std::min(to, x.size()); ++i)
            result = std::max(result, std::abs(x[i]));
        return result;
    }

    // ------------------------------------------------------------------ MIDI

    // A note-on in the middle of a host block must sound exactly there: not
    // earlier (the old chunked rendering fired it at the end of chunk 0).
    void testNoteStartsAtItsSamplePosition()
    {
        const int noteAt = warmUp + 352;
        auto synth = makeSynth(cleanPatch);
        const auto out = render(*synth, warmUp + 4 * blockSize, {{noteAt, 72}});
        // The whole signal is delayed by the (reported) oversampling latency.
        const int soundsAt = noteAt + synth->getLatencySamples();

        const float before = peak(out.left, 0, static_cast<std::size_t>(soundsAt));
        const float after = peak(out.left, static_cast<std::size_t>(soundsAt),
                                 static_cast<std::size_t>(soundsAt + 64));
        check(before == 0.0f && after > 0.0f,
              "MIDI: note-on at sample 352 of a block is silent before 352 (peak before = "
                  + juce::String(before, 5) + ")");
    }

    // With a linear patch, a chord must equal the sum of its notes played
    // alone. Repeated note-ons (one per 32-sample chunk) broke this.
    void testChordEqualsSumOfSingleNotes()
    {
        const std::vector<Note> chord {
            {warmUp + 37, 60}, {warmUp + 100, 62}, {warmUp + 352, 64}, {warmUp + 511, 65},
            {warmUp + 600, 67}, {warmUp + 1000, 69}, {warmUp + 1200, 71}, {warmUp + 1500, 72}};
        const int length = warmUp + 16 * blockSize;

        auto synth = makeSynth(cleanPatch);
        const auto together = render(*synth, length, chord);

        std::vector<double> sum(together.left.size(), 0.0);
        for (const auto& note : chord)
        {
            auto single = makeSynth(cleanPatch);
            const auto alone = render(*single, length, {note});
            for (std::size_t i = 0; i < sum.size(); ++i)
                sum[i] += alone.left[i];
        }

        double maxError = 0.0;
        for (std::size_t i = 0; i < sum.size(); ++i)
            maxError = std::max(maxError, std::abs(together.left[i] - sum[i]));
        check(maxError < 1.0e-5,
              "MIDI: 8-note chord at unaligned offsets == sum of single notes (max error = "
                  + juce::String(maxError, 6) + ")");
    }

    // ------------------------------------------------------- voice stealing

    // 8 held notes + a 9th one: juce::Synthesiser steals a voice. Rendering
    // the same 8 notes without the 9th gives a bit-identical signal up to
    // the steal, so (with - without) isolates what the steal does. A hard
    // cut shows up as a one-sample step as large as a whole voice.
    void testVoiceStealIsClickFree()
    {
        const int held[] = {60, 62, 64, 65, 67, 69, 71, 72};
        float voiceAmplitude = 0.0f;
        {
            auto single = makeSynth(cleanPatch);
            const auto out = render(*single, warmUp + 24000, {{warmUp, 62}});
            voiceAmplitude = peak(out.left, static_cast<std::size_t>(warmUp + 12000));
        }

        float worstStep = 0.0f;
        for (int k = 0; k < 6; ++k)
        {
            const int stealAt = warmUp + 24000 + k * 173; // different oscillator phases
            std::vector<Note> notes;
            for (int i = 0; i < 8; ++i)
                notes.push_back({warmUp + i * 1000, held[i]});

            auto without = makeSynth(cleanPatch);
            const auto a = render(*without, stealAt + 4096, notes);
            notes.push_back({stealAt, 84});
            auto with = makeSynth(cleanPatch);
            const auto b = render(*with, stealAt + 4096, notes);

            const int heardAt = stealAt + with->getLatencySamples();
            for (std::size_t i = static_cast<std::size_t>(heardAt);
                 i < static_cast<std::size_t>(heardAt + 4); ++i)
            {
                const float d0 = b.left[i - 1] - a.left[i - 1];
                const float d1 = b.left[i] - a.left[i];
                worstStep = std::max(worstStep, std::abs(d1 - d0));
            }
        }

        check(worstStep < 0.35f * voiceAmplitude,
              "Voice steal: largest one-sample step " + juce::String(worstStep, 4)
                  + " < 35% of one voice's amplitude (" + juce::String(voiceAmplitude, 4) + ")");
    }

    // ---------------------------------------------------- wavetable memory

    struct FakeSnapshot
    {
        std::uint64_t id = 0;
    };

    // The reclamation rules of SnapshotStore, without any audio.
    void testSnapshotStoreRules()
    {
        SnapshotStore<FakeSnapshot> store;
        store.publish(std::make_unique<FakeSnapshot>());
        store.publish(std::make_unique<FakeSnapshot>());
        store.publish(std::make_unique<FakeSnapshot>());
        check(store.size() == 3, "Snapshot store: nothing is freed before the audio thread reports");

        store.reportOldestInUse(2);           // a voice still reads snapshot 2
        store.publish(std::make_unique<FakeSnapshot>());
        check(store.size() == 3, "Snapshot store: frees only snapshots older than the report (1)");

        store.reportOldestInUse(store.nextId()); // nothing in use at all
        store.reclaim();
        check(store.size() == 1, "Snapshot store: the newest snapshot is never freed");

        // Audio block loaded nothing, then a snapshot appears and another
        // one follows before the next report: the first must survive,
        // because the next block may already be reading it.
        SnapshotStore<FakeSnapshot> race;
        race.reportOldestInUse(race.nextId());          // block k-1: nothing loaded
        const auto* first = race.publish(std::make_unique<FakeSnapshot>());
        race.publish(std::make_unique<FakeSnapshot>()); // before block k reports
        bool firstAlive = race.size() == 2;
        check(firstAlive && first->id == 1,
              "Snapshot store: a snapshot published after the last report is kept");
    }

    juce::File writeTestWavetable(int frames,
                                  std::function<float(int frame, double phase)> shape = {})
    {
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("ObjectSynthEngineTests_wavetable.wav");
        file.deleteFile();

        constexpr int frameSize = static_cast<int>(WavetableBank::tableSize);
        juce::AudioBuffer<float> audio(1, frames * frameSize);
        for (int f = 0; f < frames; ++f)
            for (int i = 0; i < frameSize; ++i)
            {
                const double phase = juce::MathConstants<double>::twoPi * i / frameSize;
                const float value = shape ? shape(f, phase / juce::MathConstants<double>::twoPi)
                                          : static_cast<float>(
                    0.6 * std::sin(phase) + 0.3 * std::sin((2 + f % 5) * phase));
                audio.setSample(0, f * frameSize + i, value);
            }

        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
        auto writer = juce::WavAudioFormat().createWriterFor(
            stream, juce::AudioFormatWriterOptions {}.withSampleRate(sampleRate)
                                                     .withNumChannels(1)
                                                     .withBitsPerSample(24));
        if (writer == nullptr || ! writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()))
            return {};
        return file;
    }

    // Repeated WAV loads and preset restores used to keep every version
    // forever (+25 MB per 100 loads of a 32-frame table).
    void testWavetableMemoryIsReclaimed()
    {
        const auto wav = writeTestWavetable(32);
        check(wav.existsAsFile(), "Memory: temporary 32-frame test WAV written");
        if (! wav.existsAsFile())
            return;

        auto synth = makeSynth(cleanPatch);
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer noMidi;
        juce::String message;

        for (int i = 0; i < 50; ++i)
        {
            synth->loadCustomWav(wav, message);
            synth->processBlock(buffer, noMidi); // the audio thread reports
        }
        check(synth->getRetainedWavetableCount() <= 2,
              "Memory: 50 WAV loads keep <= 2 snapshots (kept: "
                  + juce::String(static_cast<int>(synth->getRetainedWavetableCount())) + ")");

        juce::MemoryBlock state;
        synth->getStateInformation(state);
        const auto before = synth->getRetainedWavetableCount();
        for (int i = 0; i < 50; ++i)
        {
            synth->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            synth->processBlock(buffer, noMidi);
        }
        check(synth->getRetainedWavetableCount() == before && synth->hasCustomWavetable(),
              "Memory: restoring an identical preset 50 times re-uses the current snapshot");

        wav.deleteFile();
    }

    // Loading a new table while a note is held: the note crossfades to the
    // new table, and the old snapshot is freed once the crossfade is over,
    // even though the note is still sounding.
    void testHeldNoteFollowsNewWavetable()
    {
        const auto wav = writeTestWavetable(4);
        if (! wav.existsAsFile())
            return check(false, "Memory: temporary test WAV written");

        auto synth = makeSynth([](ObjectSynthAudioProcessor& p)
        {
            cleanPatch(p);
            set(p, "release", 0.05f);
        });
        juce::String message;
        synth->loadCustomWav(wav, message); // A; also selects Custom

        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer noteOn, noMidi;
        noteOn.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
        synth->processBlock(buffer, noteOn);

        for (int i = 0; i < 5; ++i)
        {
            synth->loadCustomWav(wav, message); // B1..B5, each a new snapshot
            synth->processBlock(buffer, noMidi); // 512 samples > 480-sample crossfade
            synth->processBlock(buffer, noMidi);
        }
        synth->loadCustomWav(wav, message);
        const auto whileHeld = synth->getRetainedWavetableCount();
        const bool sounding = buffer.getMagnitude(0, 0, blockSize) > 0.0f;

        check(whileHeld <= 2 && sounding,
              "Memory: a held note follows newly loaded tables and the old ones are freed "
              "while it still sounds (kept: " + juce::String(static_cast<int>(whileHeld)) + ")");
        wav.deleteFile();
    }

    // ------------------------------------------------- levels & distortion

    double rms(const std::vector<float>& x, std::size_t from, std::size_t to)
    {
        double sum = 0.0;
        for (std::size_t i = from; i < to; ++i)
            sum += static_cast<double>(x[i]) * x[i];
        return std::sqrt(sum / static_cast<double>(to - from));
    }

    double decibels(double ratio) { return 20.0 * std::log10(std::max(ratio, 1.0e-12)); }

    struct Spectrum
    {
        std::vector<float> magnitude;
        double binHz = 0.0;

        float around(double hz, int bins = 3) const
        {
            const int centre = static_cast<int>(std::lround(hz / binHz));
            float result = 0.0f;
            for (int b = std::max(1, centre - bins);
                 b <= std::min(static_cast<int>(magnitude.size()) - 1, centre + bins); ++b)
                result = std::max(result, magnitude[static_cast<std::size_t>(b)]);
            return result;
        }
    };

    Spectrum spectrum(const std::vector<float>& x, std::size_t from)
    {
        constexpr int order = 14;
        constexpr int size = 1 << order;
        juce::dsp::FFT fft(order);
        std::vector<float> data(2 * size, 0.0f);
        for (int i = 0; i < size; ++i)
        {
            const float window = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (size - 1));
            data[static_cast<std::size_t>(i)] = x[from + static_cast<std::size_t>(i)] * window;
        }
        fft.performFrequencyOnlyForwardTransform(data.data());
        Spectrum result;
        result.magnitude.assign(data.begin(), data.begin() + size / 2);
        result.binHz = sampleRate / size;
        return result;
    }

    // Total harmonic distortion of a steady tone at f0, as a ratio.
    double thd(const Spectrum& s, double f0)
    {
        const double fundamental = s.around(f0);
        double harmonics = 0.0;
        for (int k = 2; k * f0 < sampleRate / 2 - 200.0; ++k)
            harmonics += std::pow(static_cast<double>(s.around(k * f0)), 2.0);
        return std::sqrt(harmonics) / fundamental;
    }

    // Largest component that is NOT a harmonic of f0, relative to f0 (dB).
    double worstNonHarmonicDb(const Spectrum& s, double f0)
    {
        float worst = 0.0f;
        for (std::size_t b = 20; b < s.magnitude.size(); ++b)
        {
            const double hz = static_cast<double>(b) * s.binHz;
            const double harmonic = hz / f0;
            if (std::abs(harmonic - std::round(harmonic)) * f0 < 8.0 * s.binHz)
                continue;
            worst = std::max(worst, s.magnitude[b]);
        }
        return decibels(worst / s.around(f0));
    }

    void testNoteLevel()
    {
        auto synth = makeSynth(cleanPatch);
        const auto out = render(*synth, warmUp + 9600, {{warmUp, 69}});
        const float level = peak(out.left, static_cast<std::size_t>(warmUp));
        check(level > 0.22f && level < 0.27f,
              "Levels: one full-velocity note peaks at about -12 dBFS (peak "
                  + juce::String(level, 3) + " = " + juce::String(decibels(level), 1) + " dBFS)");
    }

    void testUnisonKeepsLoudness()
    {
        double level[2] {};
        for (int k = 0; k < 2; ++k)
        {
            auto synth = makeSynth([k](ObjectSynthAudioProcessor& p)
            {
                cleanPatch(p);
                p.setWaveform(Waveform::Sawtooth);
                set(p, "unisonEnabled", 1.0f);
                set(p, "unisonCount", k == 0 ? 1.0f : 7.0f);
                set(p, "unisonDetune", 20.0f);
            });
            const auto out = render(*synth, warmUp + 96000, {{warmUp, 57}});
            level[k] = rms(out.left, static_cast<std::size_t>(warmUp + 9600), out.left.size());
        }
        const double difference = decibels(level[1] / level[0]);
        check(std::abs(difference) < 1.5,
              "Levels: 7 unison oscillators vs 1: " + juce::String(difference, 1)
                  + " dB (was -8.6 dB)");
    }

    void testDriveChangesToneNotLoudness()
    {
        const double f0 = juce::MidiMessage::getMidiNoteInHertz(57);
        double loudness[3] {}, distortion[3] {};
        const float drives[] = {1.0f, 3.0f, 10.0f};
        for (int k = 0; k < 3; ++k)
        {
            const float driveAmount = drives[k];
            auto synth = makeSynth([driveAmount](ObjectSynthAudioProcessor& p)
            {
                cleanPatch(p);
                set(p, "distortionEnabled", 1.0f);
                set(p, "distortionMix", 1.0f);
                set(p, "distortionDrive", driveAmount);
            });
            const auto out = render(*synth, warmUp + 48000, {{warmUp, 57}});
            const auto from = static_cast<std::size_t>(warmUp + 12000);
            loudness[k] = rms(out.left, from, out.left.size());
            distortion[k] = thd(spectrum(out.left, from), f0);
        }
        const double swing = decibels(loudness[2] / loudness[0]);
        check(std::abs(swing) < 2.0,
              "Distortion: drive 1 -> 10 changes loudness by only " + juce::String(swing, 1)
                  + " dB (was about +11 dB at drive 3)");
        check(distortion[0] < 0.03 && distortion[1] > 3.0 * distortion[0] && distortion[2] > 0.15,
              "Distortion: THD rises with drive: " + juce::String(distortion[0] * 100.0, 1) + "% / "
                  + juce::String(distortion[1] * 100.0, 1) + "% / "
                  + juce::String(distortion[2] * 100.0, 1) + "% at drive 1 / 3 / 10");
    }

    // ------------------------------------------------- band-limited tables

    void testBandLimitedWavetableLevels()
    {
        // Harmonic 1 and harmonic 300 at equal amplitude.
        BandLimitedWavetable::Table source {};
        for (std::size_t i = 0; i < source.size(); ++i)
        {
            const double phase = juce::MathConstants<double>::twoPi * static_cast<double>(i) / source.size();
            source[i] = static_cast<float>(0.5 * std::sin(phase) + 0.5 * std::sin(300.0 * phase));
        }
        BandLimitedWavetable table(source);

        check(table.level(0) == source, "Band-limit: level 0 is the source table, unchanged");

        // Level keeping up to harmonic 256 must drop harmonic 300, keep harmonic 1.
        const auto& filtered = table.level(4);
        double error = 0.0;
        for (std::size_t i = 0; i < source.size(); ++i)
        {
            const double phase = juce::MathConstants<double>::twoPi * static_cast<double>(i) / source.size();
            error = std::max(error, std::abs(filtered[i] - 0.5 * std::sin(phase)));
        }
        check(BandLimitedWavetable::harmonicLimits[4] == 256 && error < 1.0e-5,
              "Band-limit: harmonics above the level limit are removed, the rest kept (error "
                  + juce::String(error, 7) + ")");

        bool safe = true;
        for (int note = 0; note <= 127; ++note)
            for (const double rate : {44100.0, 48000.0, 96000.0})
            {
                const double cycles = juce::MidiMessage::getMidiNoteInHertz(note) / rate;
                const auto level = BandLimitedWavetable::levelFor(cycles);
                safe = safe && (level + 1 == BandLimitedWavetable::levelCount
                                || BandLimitedWavetable::harmonicLimits[level] * cycles
                                       <= BandLimitedWavetable::maxPartial);
            }
        check(safe, "Band-limit: for every MIDI note at 44.1/48/96 kHz no kept harmonic exceeds 0.45 fs");
    }

    // The distortion heard above ~C5: harmonics past Nyquist folding back.
    void testOscillatorAliasing()
    {
        const auto wav = writeTestWavetable(2, [](int, double phase)
        {
            return static_cast<float>(2.0 * phase - 1.0); // naive saw: maximal harmonics
        });
        const char* names[] = {"Saw", "Square", "Custom (saw WAV)"};
        for (int w = 0; w < 3; ++w)
        {
            double worst = -1000.0;
            for (const int note : {84, 96}) // keyboard C5 and C6
            {
                auto synth = makeSynth([](ObjectSynthAudioProcessor& p)
                {
                    cleanPatch(p);
                    set(p, "amplitudeEnvelopeEnabled", 0.0f);
                });
                juce::String message;
                if (w == 2)
                    synth->loadCustomWav(wav, message);
                synth->setWaveform(w == 0 ? Waveform::Sawtooth
                                          : w == 1 ? Waveform::Square : Waveform::Custom);
                const auto out = render(*synth, warmUp + 48000, {{warmUp, note}});
                const double f0 = juce::MidiMessage::getMidiNoteInHertz(note);
                worst = std::max(worst, worstNonHarmonicDb(
                    spectrum(out.left, static_cast<std::size_t>(warmUp + 12000)), f0));
            }
            check(worst < -60.0, juce::String("Aliasing: ") + names[w]
                                     + " at C5/C6, strongest inharmonic component "
                                     + juce::String(worst, 1) + " dB (was -27 dB for Saw at C5)");
        }
        wav.deleteFile();
    }

    void testBandLimitingKeepsLoudness()
    {
        double level[2] {};
        const int notes[] = {60, 84};
        for (int k = 0; k < 2; ++k)
        {
            auto synth = makeSynth([](ObjectSynthAudioProcessor& p)
            {
                cleanPatch(p);
                set(p, "amplitudeEnvelopeEnabled", 0.0f);
                p.setWaveform(Waveform::Sawtooth);
            });
            const auto out = render(*synth, warmUp + 24000, {{warmUp, notes[k]}});
            level[k] = rms(out.left, static_cast<std::size_t>(warmUp + 4800), out.left.size());
        }
        const double difference = decibels(level[1] / level[0]);
        check(std::abs(difference) < 1.0,
              "Band-limit: a saw at C5 is as loud as at C3 (" + juce::String(difference, 2) + " dB)");
    }

    void testDistortionAliasing()
    {
        const int note = 93; // A6, 1760 Hz: many harmonics above Nyquist
        const double f0 = juce::MidiMessage::getMidiNoteInHertz(note);
        auto synth = makeSynth([](ObjectSynthAudioProcessor& p)
        {
            cleanPatch(p);
            set(p, "distortionEnabled", 1.0f);
            set(p, "distortionMix", 1.0f);
            set(p, "distortionDrive", 10.0f);
        });
        const auto out = render(*synth, warmUp + 48000, {{warmUp, note}});
        const double alias = worstNonHarmonicDb(spectrum(out.left, static_cast<std::size_t>(warmUp + 12000)), f0);
        check(alias < -50.0,
              "Distortion: strongest aliased component at drive 10 is " + juce::String(alias, 1)
                  + " dB below the fundamental (oversampled) — latency "
                  + juce::String(synth->getLatencySamples()) + " samples");
    }

    void testDefaultPatchChordHasHeadroom()
    {
        auto synth = makeSynth({});
        std::vector<Note> chord;
        for (int n : {48, 52, 55, 60, 64, 67, 72, 76})
            chord.push_back({warmUp, n, true, 100.0f / 127.0f});
        const auto out = render(*synth, warmUp + 48000, chord);
        const float level = std::max(peak(out.left), peak(out.right));
        check(level < 1.0f,
              "Levels: default patch, 8-note chord at velocity 100 does not clip (peak "
                  + juce::String(decibels(level), 1) + " dBFS)");
    }

    // ---------------------------------------------------------------- stereo

    double correlation(const Stereo& s, std::size_t from)
    {
        double lr = 0.0, ll = 0.0, rr = 0.0;
        for (std::size_t i = from; i < s.left.size(); ++i)
        {
            lr += static_cast<double>(s.left[i]) * s.right[i];
            ll += static_cast<double>(s.left[i]) * s.left[i];
            rr += static_cast<double>(s.right[i]) * s.right[i];
        }
        return lr / std::sqrt(ll * rr + 1.0e-30);
    }

    Stereo renderUnison(float spread)
    {
        auto synth = makeSynth([spread](ObjectSynthAudioProcessor& p)
        {
            cleanPatch(p);
            p.setWaveform(Waveform::Sawtooth);
            set(p, "unisonEnabled", 1.0f);
            set(p, "unisonCount", 7.0f);
            set(p, "unisonDetune", 20.0f);
            set(p, "unisonSpread", spread);
        });
        return render(*synth, warmUp + 48000, {{warmUp, 57}});
    }

    void testUnisonSpread()
    {
        const auto mono = renderUnison(0.0f);
        float difference = 0.0f;
        for (std::size_t i = 0; i < mono.left.size(); ++i)
            difference = std::max(difference, std::abs(mono.left[i] - mono.right[i]));
        check(difference == 0.0f, "Stereo: Spread 0 with chorus off gives identical L and R");

        const auto wide = renderUnison(1.0f);
        const double c = correlation(wide, static_cast<std::size_t>(warmUp + 4800));
        check(c < 0.9, "Stereo: Spread 1 decorrelates L and R (correlation "
                           + juce::String(c, 2) + ", 1.00 = mono)");
    }

    void testChorusIsStereo()
    {
        auto synth = makeSynth([](ObjectSynthAudioProcessor& p)
        {
            cleanPatch(p);
            p.setWaveform(Waveform::Sawtooth);
            set(p, "chorusEnabled", 1.0f);
            set(p, "chorusMix", 0.5f);
            set(p, "chorusDepth", 0.5f);
        });
        const auto out = render(*synth, warmUp + 48000, {{warmUp, 57}});
        const double c = correlation(out, static_cast<std::size_t>(warmUp + 4800));
        check(c < 0.99, "Stereo: the chorus alone makes L != R (correlation " + juce::String(c, 3) + ")");
    }

    void testPanExtremes()
    {
        for (const float pan : {-1.0f, 1.0f})
        {
            auto synth = makeSynth([pan](ObjectSynthAudioProcessor& p)
            {
                cleanPatch(p);
                set(p, "outputPan", pan);
            });
            const auto out = render(*synth, warmUp + 9600, {{warmUp, 69}});
            const float kept = peak(pan < 0 ? out.left : out.right);
            const float muted = peak(pan < 0 ? out.right : out.left);
            check(kept > 0.2f && muted < 1.0e-6f,
                  juce::String("Stereo: Pan ") + (pan < 0 ? "hard left" : "hard right")
                      + " silences the other side and keeps unity on its own");
        }
    }

    void testLfoAutoPan()
    {
        auto synth = makeSynth([](ObjectSynthAudioProcessor& p)
        {
            cleanPatch(p);
            set(p, "lfoEnabled", 1.0f);
            // Tempo-synced rate: 1/4 at the 120 BPM fallback (no host) = 2 Hz.
            set(p, "lfoSync", static_cast<float>(TempoSync::defaultIndex));
            set(p, "lfoWaveform", 0.0f); // sine
            set(p, "lfoRoute1Destination", static_cast<float>(ModDestination::OutputPan));
            set(p, "lfoRoute1Depth", 1.0f);
        });
        const auto out = render(*synth, warmUp + 48000, {{warmUp, 69}});
        double lowest = 1.0e9, highest = 0.0;
        for (std::size_t from = static_cast<std::size_t>(warmUp + 4800); from + 2400 <= out.left.size(); from += 2400)
        {
            const double ratio = rms(out.left, from, from + 2400) / (rms(out.right, from, from + 2400) + 1.0e-9);
            lowest = std::min(lowest, ratio);
            highest = std::max(highest, ratio);
        }
        check(highest > 4.0 && lowest < 0.25,
              "Stereo: LFO -> Pan sweeps the sound left and right (L/R ratio "
                  + juce::String(lowest, 2) + " .. " + juce::String(highest, 1) + ")");
    }

    void testMonoOutputLayout()
    {
        auto synth = makeSynth(cleanPatch, 1);
        const auto out = render(*synth, warmUp + 9600, {{warmUp, 69}});
        const float level = peak(out.left, static_cast<std::size_t>(warmUp));
        check(synth->getTotalNumOutputChannels() == 1 && level > 0.22f && level < 0.27f,
              "Stereo: a mono output bus still renders at the normal level (peak "
                  + juce::String(level, 3) + ")");
    }

    // ----------------------------------------------------- live changes

    float maxStep(const std::vector<float>& x, std::size_t from, std::size_t to)
    {
        float result = 0.0f;
        for (std::size_t i = std::max<std::size_t>(from, 1); i < std::min(to, x.size()); ++i)
            result = std::max(result, std::abs(x[i] - x[i - 1]));
        return result;
    }

    // A click is a one-sample jump much larger than the waveform's own
    // steepest step. Each change below is made at several moments (different
    // phases) and the worst transition is compared with the steady signal.
    struct ClickCheck
    {
        float transition = 0.0f, steady = 0.0f;
        bool ok(float factor = 2.0f) const { return transition <= factor * steady; }
        juce::String describe() const
        {
            return "worst step " + juce::String(transition, 4) + " vs steady " + juce::String(steady, 4);
        }
    };

    ClickCheck measureClicks(const Setup& setup, int note,
                             const std::function<void(ObjectSynthAudioProcessor&)>& change)
    {
        ClickCheck result;
        for (int k = 0; k < 4; ++k)
        {
            const int changeAt = warmUp + 12288 + k * 3 * blockSize; // block-aligned
            auto synth = makeSynth(setup);
            const auto out = render(*synth, changeAt + 24000, {{warmUp, note}}, {{changeAt, change}});
            const auto at = static_cast<std::size_t>(changeAt + synth->getLatencySamples());
            for (const auto* channel : {&out.left, &out.right})
            {
                result.transition = std::max(result.transition, maxStep(*channel, at - 4, at + 960));
                result.steady = std::max({result.steady, maxStep(*channel, at - 4800, at - 8),
                                          maxStep(*channel, at + 4800, at + 9600)});
            }
        }
        return result;
    }

    void steadySine(ObjectSynthAudioProcessor& p)
    {
        cleanPatch(p);
        set(p, "amplitudeEnvelopeEnabled", 0.0f); // constant level
    }

    void testLiveWaveformChange()
    {
        const auto clicks = measureClicks(steadySine, 57,
            [](ObjectSynthAudioProcessor& p) { p.setWaveform(Waveform::Triangle); });
        check(clicks.ok(), "Live: Sine -> Triangle on a held note is click-free (" + clicks.describe() + ")");

        // And the held note really changes: a triangle has a 3rd harmonic.
        const double f0 = juce::MidiMessage::getMidiNoteInHertz(57);
        auto synth = makeSynth(steadySine);
        const int changeAt = warmUp + 36 * blockSize; // 16384-sample window fits before it
        const auto out = render(*synth, changeAt + 48000, {{warmUp, 57}},
            {{changeAt, [](ObjectSynthAudioProcessor& p) { p.setWaveform(Waveform::Triangle); }}});
        const auto before = spectrum(out.left, static_cast<std::size_t>(changeAt - 16384 - blockSize));
        const auto after = spectrum(out.left, static_cast<std::size_t>(changeAt + 4800));
        const double thirdBefore = decibels(before.around(3 * f0) / before.around(f0));
        const double thirdAfter = decibels(after.around(3 * f0) / after.around(f0));
        check(thirdBefore < -50.0 && thirdAfter > -25.0,
              "Live: the held note becomes a triangle (3rd harmonic " + juce::String(thirdBefore, 0)
                  + " dB -> " + juce::String(thirdAfter, 0) + " dB)");
    }

    void testLiveCustomFrameChange()
    {
        // Frame 1: sine, frame 2: saw.
        const auto wav = writeTestWavetable(2, [](int frame, double phase)
        {
            return frame == 0 ? static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * phase))
                              : static_cast<float>(2.0 * phase - 1.0);
        });
        const double f0 = juce::MidiMessage::getMidiNoteInHertz(57);
        auto synth = makeSynth(steadySine);
        juce::String message;
        synth->loadCustomWav(wav, message);
        const int changeAt = warmUp + 36 * blockSize; // 16384-sample window fits before it
        const auto out = render(*synth, changeAt + 48000, {{warmUp, 57}},
            {{changeAt, [](ObjectSynthAudioProcessor& p) { p.selectCustomFrame(1); }}});
        const auto before = spectrum(out.left, static_cast<std::size_t>(changeAt - 16384 - blockSize));
        const auto after = spectrum(out.left, static_cast<std::size_t>(changeAt + 4800));
        const double secondBefore = decibels(before.around(2 * f0) / before.around(f0));
        const double secondAfter = decibels(after.around(2 * f0) / after.around(f0));
        check(secondBefore < -50.0 && secondAfter > -10.0,
              "Live: moving the frame slider changes the held note (2nd harmonic "
                  + juce::String(secondBefore, 0) + " dB -> " + juce::String(secondAfter, 0) + " dB)");
        wav.deleteFile();
    }

    void testLiveUnisonChange()
    {
        auto setup = [](ObjectSynthAudioProcessor& p)
        {
            steadySine(p);
            set(p, "unisonEnabled", 1.0f);
            set(p, "unisonCount", 1.0f);
            set(p, "unisonDetune", 20.0f);
            set(p, "unisonSpread", 1.0f);
        };
        auto widen = [](ObjectSynthAudioProcessor& p) { set(p, "unisonCount", 7.0f); };

        const auto clicks = measureClicks(setup, 57, widen);
        check(clicks.ok(), "Live: Unison 1 -> 7 voices on a held note is click-free (" + clicks.describe() + ")");

        auto synth = makeSynth(setup);
        const int changeAt = warmUp + 12288;
        const auto out = render(*synth, changeAt + 48000, {{warmUp, 57}}, {{changeAt, widen}});
        Stereo before, after;
        before.left.assign(out.left.begin() + warmUp + 2048, out.left.begin() + changeAt);
        before.right.assign(out.right.begin() + warmUp + 2048, out.right.begin() + changeAt);
        after.left.assign(out.left.begin() + changeAt + 4800, out.left.end());
        after.right.assign(out.right.begin() + changeAt + 4800, out.right.end());
        const double cBefore = correlation(before, 0), cAfter = correlation(after, 0);
        const double loudness = decibels(rms(after.left, 0, after.left.size())
                                         / rms(before.left, 0, before.left.size()));
        check(cBefore > 0.999 && cAfter < 0.9 && std::abs(loudness) < 2.0,
              "Live: the held note gets 7 detuned, spread voices (L/R correlation "
                  + juce::String(cBefore, 2) + " -> " + juce::String(cAfter, 2) + ", level "
                  + juce::String(loudness, 1) + " dB)");
    }

    void testLiveEnvelopeChanges()
    {
        auto setup = [](ObjectSynthAudioProcessor& p)
        {
            cleanPatch(p);
            set(p, "sustain", 0.7f);
        };
        auto lower = [](ObjectSynthAudioProcessor& p) { set(p, "sustain", 0.2f); };

        const auto clicks = measureClicks(setup, 57, lower);
        check(clicks.ok(), "Live: Sustain 0.7 -> 0.2 on a held note is click-free (" + clicks.describe() + ")");

        auto synth = makeSynth(setup);
        const int changeAt = warmUp + 12288;
        const auto out = render(*synth, changeAt + 24000, {{warmUp, 57}}, {{changeAt, lower}});
        const double ratio = rms(out.left, static_cast<std::size_t>(changeAt + 4800), static_cast<std::size_t>(changeAt + 14400))
                             / rms(out.left, static_cast<std::size_t>(changeAt - 9600), static_cast<std::size_t>(changeAt));
        check(std::abs(ratio - 0.2 / 0.7) < 0.02,
              "Live: the held note follows the new Sustain (level x" + juce::String(ratio, 3) + ", expected x0.286)");

        // juce::ADSR cut a releasing note dead when Sustain was set to 0
        // (it recomputed the release slope from the sustain level).
        auto releasing = makeSynth([](ObjectSynthAudioProcessor& p)
        {
            cleanPatch(p);
            set(p, "release", 1.0f);
        });
        const int noteOff = warmUp + 9600;
        const auto tail = render(*releasing, noteOff + 24000, {{warmUp, 57}, {noteOff, 57, false}},
            {{noteOff + 4608, [](ObjectSynthAudioProcessor& p) { set(p, "sustain", 0.0f); }}});
        const float stillThere = peak(tail.left, static_cast<std::size_t>(noteOff + 9600),
                                      static_cast<std::size_t>(noteOff + 12000));
        check(stillThere > 0.05f,
              "Live: setting Sustain to 0 during the release does not cut the note (level "
                  + juce::String(stillThere, 3) + " 0.2 s later)");
    }

    void testLiveADSRUnit()
    {
        LiveADSR adsr;
        adsr.setSampleRate(48000.0);
        adsr.setParameters(0.0f, 0.0f, 1.0f, 1.0f);
        adsr.noteOn();
        for (int i = 0; i < 100; ++i) adsr.getNextSample();
        adsr.noteOff();
        for (int i = 0; i < 24000; ++i) adsr.getNextSample(); // half of a 1 s release
        const float halfway = adsr.getLevel();
        adsr.setParameters(0.0f, 0.0f, 1.0f, 2.0f); // slower release from HERE
        const float next = adsr.getNextSample();
        int samples = 1;
        // Calling setParameters every sample with unchanged values must not
        // restart the release (it would then never finish).
        while (adsr.isActive() && samples < 200000)
        {
            adsr.setParameters(0.0f, 0.0f, 1.0f, 2.0f);
            adsr.getNextSample();
            ++samples;
        }
        check(std::abs(halfway - 0.5f) < 0.01f && std::abs(next - halfway) < 1.0e-4f
                  && std::abs(samples - 96000) < 960, // 1%: float accumulation over 96k steps
              "LiveADSR: changing Release mid-release continues from the current level and "
              "lasts the new time (" + juce::String(samples) + " samples = " + juce::String(samples / 48000.0, 3) + " s for the rest; level "
                  + juce::String(halfway, 4) + " -> " + juce::String(next, 4) + ")");
    }

    // The OUTPUT meter reads the peak after Volume, and each read resets it.
    void testOutputMeterPeak()
    {
        auto synth = makeSynth([](ObjectSynthAudioProcessor& p)
        {
            cleanPatch(p);
            set(p, "outputVolume", -6.0f);
        });
        const auto out = render(*synth, warmUp + 9600, {{warmUp, 69}});
        const float expected = std::max(peak(out.left, out.left.size() - blockSize), 1.0e-6f);
        const float first = synth->consumeOutputPeak(0);
        const float second = synth->consumeOutputPeak(0);
        check(first >= expected * 0.999f && first < 0.15f && second == 0.0f,
              "Output: meter peak follows Volume (-6 dB -> " + juce::String(first, 3)
                  + ") and resets after each read");
    }

    // Presets saved before Volume/Pan/Spread existed must load them at their
    // defaults, not keep whatever the current values are.
    void testOldPresetGetsDefaultsForNewParameters()
    {
        auto source = makeSynth({});
        juce::MemoryBlock state;
        source->getStateInformation(state);
        auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
        for (auto* id : {"outputVolume", "outputPan", "unisonSpread"})
            if (auto* entry = xml->getChildByAttribute("id", id))
                xml->removeChildElement(entry, true);
        juce::MemoryBlock oldPreset;
        juce::AudioProcessor::copyXmlToBinary(*xml, oldPreset);

        auto target = makeSynth([](ObjectSynthAudioProcessor& p)
        {
            set(p, "outputVolume", -20.0f);
            set(p, "outputPan", -1.0f);
            set(p, "unisonSpread", 0.0f);
        });
        target->setStateInformation(oldPreset.getData(), static_cast<int>(oldPreset.getSize()));

        auto value = [&target](const char* id)
        { return target->getAPVTS().getRawParameterValue(id)->load(); };
        check(std::abs(value("outputVolume") + 6.0f) < 1.0e-4f && std::abs(value("outputPan")) < 1.0e-4f
                  && std::abs(value("unisonSpread") - 0.5f) < 1.0e-4f,
              "Presets: an old preset loads Volume -6 dB, Pan centre, Spread 0.5 (defaults)");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    testNoteStartsAtItsSamplePosition();
    testChordEqualsSumOfSingleNotes();
    testVoiceStealIsClickFree();
    testSnapshotStoreRules();
    testWavetableMemoryIsReclaimed();
    testHeldNoteFollowsNewWavetable();
    testNoteLevel();
    testUnisonKeepsLoudness();
    testDriveChangesToneNotLoudness();
    testDistortionAliasing();
    testBandLimitedWavetableLevels();
    testOscillatorAliasing();
    testBandLimitingKeepsLoudness();
    testDefaultPatchChordHasHeadroom();
    testUnisonSpread();
    testChorusIsStereo();
    testPanExtremes();
    testLfoAutoPan();
    testMonoOutputLayout();
    testLiveADSRUnit();
    testLiveWaveformChange();
    testLiveCustomFrameChange();
    testLiveUnisonChange();
    testLiveEnvelopeChanges();
    testOutputMeterPeak();
    testOldPresetGetsDefaultsForNewParameters();

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

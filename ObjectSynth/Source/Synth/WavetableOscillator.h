#pragma once

#include "WavetableBank.h"
#include <cmath>
#include <cstddef>

class WavetableOscillator {
public:
    WavetableOscillator() noexcept { updateTable(); }

    explicit WavetableOscillator(const WavetableBank& data) noexcept : bank(&data) { updateTable(); }

    std::size_t getSize() const noexcept {
        return WavetableBank::tableSize;
    }

    void prepare(double newSampleRate);

    void setFrequency(double newFrequency);

    float getNextSample() noexcept;

    void setWaveform(Waveform newWaveform) noexcept {
        waveform = newWaveform;
        updateTable();
    }

    void resetPhase() noexcept {
        readPosition = 0.0;
    }

    // Phase as a fraction of one cycle, [0, 1).
    void setPhase(double cycles) noexcept {
        const double wrapped = cycles - std::floor(cycles);
        readPosition = std::isfinite(wrapped) ? wrapped * static_cast<double>(getSize()) : 0.0;
    }

    // Custom wavetable (with its band-limited levels); nullptr = built-in saw.
    void setCustomTable(const BandLimitedWavetable* table) noexcept{
        customTable = table;
        updateTable();
    }

    // When the table being played changes (waveform, custom frame or
    // band-limited level), fade from the old table to the new one over this
    // many samples, reading both at the same phase. 0 = switch at once (the
    // default, used by the standalone tests).
    void setTableCrossfade(int samples) noexcept {
        crossfadeLength = samples > 0 ? samples : 0;
        snapToCurrentTable(); // a fade in progress used the old length
    }

    // Ends any crossfade now and forgets the previous table. Used when an
    // oscillator starts sounding from silence (nothing to fade from), and
    // required before it is read again after being silent: the previous
    // table may belong to a custom snapshot that no longer exists.
    void snapToCurrentTable() noexcept {
        previousTable = nullptr;
        fadeRemaining = 0;
    }

    bool isCrossfading() const noexcept { return fadeRemaining > 0; }

private:
    // Picks the band-limited level for the current frequency and sample rate.
    // Called on every change of waveform, table, frequency or sample rate, so
    // getNextSample() just reads one table (no per-sample branching).
    void updateTable() noexcept {
        const auto& source = waveform == Waveform::Custom && customTable != nullptr
            ? *customTable : bank->getBandLimited(waveform);
        const auto* newTable = &source.forFrequency(sampleRate > 0.0 ? frequency / sampleRate : 0.0);
        if (newTable == table)
            return;
        // Only pointers are stored here; nothing is read until getNextSample().
        if (table != nullptr && crossfadeLength > 0)
        {
            previousTable = table;
            fadeRemaining = crossfadeLength;
        }
        table = newTable;
    }

    const WavetableBank* bank = &WavetableBank::builtIn();
    const BandLimitedWavetable* customTable = nullptr;
    const WavetableBank::Table* table = nullptr; // level currently played
    const WavetableBank::Table* previousTable = nullptr; // faded out, while fadeRemaining > 0
    int crossfadeLength = 0;
    int fadeRemaining = 0;

    Waveform waveform = Waveform::Sawtooth;

    double sampleRate = 48000.0;
    double frequency = 0.0;

    double readPosition = 0.0;
    double readIncrement = 0.0;
};

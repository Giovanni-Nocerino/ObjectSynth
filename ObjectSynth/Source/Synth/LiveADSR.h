#pragma once

#include <algorithm>
#include <cmath>

// Linear ADSR with the same shape as juce::ADSR, but safe to edit while a
// note is sounding. juce::ADSR was designed to be configured at Note On:
// changing it mid-note recomputes the release slope from the sustain level
// (with Sustain = 0 a releasing note is cut off at once) and a new Sustain
// value is applied as a jump. Here:
//  - Attack / Decay / Release changes only alter the slope of the stage in
//    progress, starting from the CURRENT level (no jump).
//  - The Sustain level is approached with a short glide (no click).
//  - noteOn() from a non-zero level (voice stealing) attacks from that level.
// Rates are recomputed only for the values that changed, so calling
// setParameters() every block is cheap and never restarts a release.
// No JUCE dependency.
class LiveADSR final
{
public:
    enum class Stage { idle, attack, decay, sustain, release };

    void setSampleRate(double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        glide = static_cast<float>(1.0 - std::exp(-1.0 / (sustainGlideSeconds * sampleRate)));
        attackRate = rateFor(1.0f, attackSeconds);
        decayRate = rateFor(1.0f - sustainLevel, decaySeconds);
        if (stage == Stage::release)
            releaseRate = rateFor(level, releaseSeconds);
    }

    void setParameters(float attack, float decay, float sustain, float release) noexcept
    {
        attack = std::max(0.0f, attack);
        decay = std::max(0.0f, decay);
        sustain = std::clamp(sustain, 0.0f, 1.0f);
        release = std::max(0.0f, release);

        if (attack != attackSeconds)
        {
            attackSeconds = attack;
            attackRate = rateFor(1.0f, attackSeconds);
            if (stage == Stage::attack && attackRate <= 0.0f)
                finishAttack();
        }

        if (decay != decaySeconds || sustain != sustainLevel)
        {
            decaySeconds = decay;
            sustainLevel = sustain;
            decayRate = rateFor(1.0f - sustainLevel, decaySeconds);
            if (stage == Stage::decay && (decayRate <= 0.0f || level <= sustainLevel))
                stage = Stage::sustain; // the glide takes it to the new level
        }

        if (release != releaseSeconds)
        {
            releaseSeconds = release;
            if (stage == Stage::release)
            {
                // New release time counts from where the level is now.
                releaseRate = rateFor(level, releaseSeconds);
                if (releaseRate <= 0.0f)
                    reset();
            }
        }
    }

    void noteOn() noexcept
    {
        if (attackRate > 0.0f)
        {
            stage = Stage::attack;
        }
        else if (decayRate > 0.0f)
        {
            level = 1.0f;
            stage = Stage::decay;
        }
        else
        {
            level = sustainLevel;
            stage = Stage::sustain;
        }
    }

    void noteOff() noexcept
    {
        if (stage == Stage::idle)
            return;
        releaseRate = rateFor(level, releaseSeconds);
        if (releaseRate > 0.0f)
            stage = Stage::release;
        else
            reset();
    }

    float getNextSample() noexcept
    {
        switch (stage)
        {
            case Stage::idle:
                return 0.0f;

            case Stage::attack:
                level += attackRate;
                if (level >= 1.0f)
                    finishAttack();
                break;

            case Stage::decay:
                level -= decayRate;
                if (level <= sustainLevel)
                {
                    level = sustainLevel;
                    stage = Stage::sustain;
                }
                break;

            case Stage::sustain:
                level += (sustainLevel - level) * glide;
                break;

            case Stage::release:
                level -= releaseRate;
                if (level <= 0.0f)
                    reset();
                break;
        }
        return level;
    }

    void reset() noexcept
    {
        stage = Stage::idle;
        level = 0.0f;
    }

    bool isActive() const noexcept { return stage != Stage::idle; }
    Stage getStage() const noexcept { return stage; }
    float getLevel() const noexcept { return level; }

private:
    static constexpr double sustainGlideSeconds = 0.01;

    float rateFor(float distance, float seconds) const noexcept
    {
        return seconds > 0.0f
            ? static_cast<float>(static_cast<double>(distance) / (static_cast<double>(seconds) * sampleRate))
            : -1.0f;
    }

    void finishAttack() noexcept
    {
        level = 1.0f;
        stage = decayRate > 0.0f ? Stage::decay : Stage::sustain;
    }

    double sampleRate = 48000.0;
    float attackSeconds = 0.01f, decaySeconds = 0.1f, sustainLevel = 0.7f, releaseSeconds = 0.2f;
    float attackRate = -1.0f, decayRate = -1.0f, releaseRate = -1.0f;
    float glide = 0.0f;
    float level = 0.0f;
    Stage stage = Stage::idle;
};

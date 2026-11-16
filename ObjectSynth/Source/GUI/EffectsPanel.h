#pragma once

#include "Theme.h"
#include "../DSP/Saturator.h"

namespace osui
{
    // Distortion and Chorus live in the tall effects column on the right,
    // so they use the same stacked knobs as the other panels.
    class DistortionPanel final : public ModulePanel
    {
    public:
        explicit DistortionPanel(APVTS& state)
            : ModulePanel("DIST.")
        {
            setupKnob(*this, knobs[0], state, "distortionDrive", "DRIVE", orange());
            setupKnob(*this, knobs[1], state, "distortionMix", "DRY / WET", orange());

            // The 1..10 knob is a gain of 0..+36 dB into the clipper
            // (Saturator::driveToDecibels): show what it really does.
            // Set after the attachment, which installs its own text functions.
            auto& drive = knobs[0].slider;
            drive.textFromValueFunction = [](double value)
            {
                return "+" + juce::String(Saturator::driveToDecibels(static_cast<float>(value)), 1) + " dB";
            };
            drive.valueFromTextFunction = [](const juce::String& text)
            {
                const float decibels = text.retainCharacters("0123456789.-").getFloatValue();
                return 1.0 + 9.0 * juce::jlimit(0.0, 1.0,
                                                static_cast<double>(decibels / Saturator::maxDriveDecibels));
            };
            drive.updateText();
            drive.setTooltip("Gain into the soft clipper. Loudness is compensated: "
                             "more drive = more harmonics, not more volume.");

            setupPowerButton(*this, power, powerAttachment, state, "distortionEnabled");
            setPowerIndicator(power);
        }

        void resized() override
        {
            power.setBounds(getWidth() - 70, 9, 55, 22);
            auto area = getBodyBounds();
            const int width = area.getWidth() / 2;
            positionKnob(knobs[0], area.removeFromLeft(width), 56);
            positionKnob(knobs[1], area, 56);
        }

    private:
        std::array<Knob, 2> knobs;
        PowerButton power;
        std::unique_ptr<ButtonAttachment> powerAttachment;
    };

    class ChorusPanel final : public ModulePanel
    {
    public:
        explicit ChorusPanel(APVTS& state)
            : ModulePanel("CHORUS")
        {
            setupKnob(*this, knobs[0], state, "chorusRate", "RATE", purple(), " Hz");
            setupKnob(*this, knobs[1], state, "chorusDepth", "DEPTH", purple());
            setupKnob(*this, knobs[2], state, "chorusDelay", "DELAY", purple(), " ms");
            setupKnob(*this, knobs[3], state, "chorusMix", "DRY / WET", purple());
            setupPowerButton(*this, power, powerAttachment, state, "chorusEnabled");
            setPowerIndicator(power);
        }

        void resized() override
        {
            power.setBounds(getWidth() - 70, 9, 55, 22);
            auto area = getBodyBounds();
            const int rowHeight = area.getHeight() / 2;

            for (int row = 0; row < 2; ++row)
            {
                auto currentRow = row == 0 ? area.removeFromTop(rowHeight) : area;
                const int knobWidth = currentRow.getWidth() / 2;

                for (int col = 0; col < 2; ++col)
                {
                    auto cell = col == 0 ? currentRow.removeFromLeft(knobWidth) : currentRow;
                    positionKnob(knobs[static_cast<std::size_t>(row * 2 + col)], cell, 56);
                }
            }
        }

    private:
        std::array<Knob, 4> knobs;
        PowerButton power;
        std::unique_ptr<ButtonAttachment> powerAttachment;
    };
}

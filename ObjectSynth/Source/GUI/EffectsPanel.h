#pragma once

#include "Theme.h"

namespace osui
{
    // Sketch: Distortion and Chorus share the narrow lower-left column, so
    // both use inline knobs (dial left, caption + value right).
    class DistortionPanel final : public ModulePanel
    {
    public:
        explicit DistortionPanel(APVTS& state)
            : ModulePanel("DIST.")
        {
            setupKnob(*this, knobs[0], state, "distortionDrive", "DRIVE", orange(), " x");
            setupKnob(*this, knobs[1], state, "distortionMix", "DRY / WET", orange());
            for (auto& knob : knobs)
                makeKnobInline(knob, 66);
            setupPowerButton(*this, power, powerAttachment, state, "distortionEnabled");
            setPowerIndicator(power);
        }

        void resized() override
        {
            power.setBounds(getWidth() - 70, 9, 55, 22);
            auto area = getBodyBounds();
            const int width = area.getWidth() / 2;
            positionKnobInline(knobs[0], area.removeFromLeft(width), 46);
            positionKnobInline(knobs[1], area, 46);
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
            for (auto& knob : knobs)
                makeKnobInline(knob, 66);
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
                    positionKnobInline(knobs[static_cast<std::size_t>(row * 2 + col)], cell, 46);
                }
            }
        }

    private:
        std::array<Knob, 4> knobs;
        PowerButton power;
        std::unique_ptr<ButtonAttachment> powerAttachment;
    };
}

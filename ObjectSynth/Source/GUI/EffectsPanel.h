#pragma once

#include "Theme.h"

namespace osui
{
    class DistortionPanel final : public ModulePanel
    {
    public:
        explicit DistortionPanel(APVTS& state)
            : ModulePanel("DISTORTION")
        {
            setupKnob(*this, knobs[0], state, "distortionDrive", "DRIVE", orange(), " x");
            setupKnob(*this, knobs[1], state, "distortionMix", "DRY / WET", orange());
            setupPowerButton(*this, power, powerAttachment, state, "distortionEnabled");
        }

        void resized() override
        {
            power.setBounds(getWidth() - 64, 9, 50, 22);
            auto area = getBodyBounds();
            const int width = area.getWidth() / 2;
            positionKnob(knobs[0], area.removeFromLeft(width));
            positionKnob(knobs[1], area);
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
        }

        void resized() override
        {
            power.setBounds(getWidth() - 64, 9, 50, 22);
            auto area = getBodyBounds();
            const int rowHeight = area.getHeight() / 2;
            
            for (int row = 0; row < 2; ++row) {
                
                auto currentRow = row == 0 ? area.removeFromTop(rowHeight) : area;
                const int knobWidth = currentRow.getWidth() / 2;

                for (int col = 0; col < 2; ++col) {
                    auto cell = col == 0 ? currentRow.removeFromLeft(knobWidth) : currentRow;
                    positionKnob(knobs[row * 2 + col], cell);
                }
            }
        }

    private:
        std::array<Knob, 4> knobs;
        PowerButton power;
        std::unique_ptr<ButtonAttachment> powerAttachment;
    };
}

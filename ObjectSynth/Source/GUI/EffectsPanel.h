#pragma once

#include "Theme.h"

namespace osui
{
    class DistortionPanel final : public ModulePanel
    {
    public:
        explicit DistortionPanel(APVTS& state)
            : ModulePanel("DISTORTION", "SOFT SATURATION")
        {
            setupKnob(*this, knobs[0], state, "distortionDrive", "DRIVE", orange(), " x");
            setupKnob(*this, knobs[1], state, "distortionMix", "DRY / WET", orange());
        }

        void resized() override
        {
            auto area = getBodyBounds();
            const int width = area.getWidth() / 2;
            positionKnob(knobs[0], area.removeFromLeft(width));
            positionKnob(knobs[1], area);
        }

    private:
        std::array<Knob, 2> knobs;
    };

    class ChorusPanel final : public ModulePanel
    {
    public:
        explicit ChorusPanel(APVTS& state)
            : ModulePanel("CHORUS", "MODULATED DELAY")
        {
            setupKnob(*this, knobs[0], state, "chorusRate", "RATE", purple(), " Hz");
            setupKnob(*this, knobs[1], state, "chorusDepth", "DEPTH", purple());
            setupKnob(*this, knobs[2], state, "chorusDelay", "DELAY", purple(), " ms");
            setupKnob(*this, knobs[3], state, "chorusMix", "DRY / WET", purple());
        }

        void resized() override
        {
            auto area = getBodyBounds();
            const int width = area.getWidth() / static_cast<int>(knobs.size());
            for (std::size_t i = 0; i < knobs.size(); ++i)
            {
                auto cell = i + 1 == knobs.size() ? area : area.removeFromLeft(width);
                positionKnob(knobs[i], cell);
            }
        }

    private:
        std::array<Knob, 4> knobs;
    };
}

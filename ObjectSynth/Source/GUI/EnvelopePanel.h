#pragma once

#include "Theme.h"
#include <algorithm>

namespace osui
{
    class EnvelopePanel final : public ModulePanel, private juce::Timer
    {
    public:
        explicit EnvelopePanel(APVTS& state)
            : ModulePanel("ENVELOPE", "AMPLITUDE / ADSR")
        {
            constexpr const char* ids[] = {"attack", "decay", "sustain", "release"};
            constexpr const char* names[] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
            for (std::size_t i = 0; i < knobs.size(); ++i)
                setupKnob(*this, knobs[i], state, ids[i], names[i], cyan(),
                          i == 2 ? "" : " s");
            startTimerHz(24); // Host automation may change values outside the editor.
        }

        void paint(juce::Graphics& g) override
        {
            ModulePanel::paint(g);
            if (plotArea.isEmpty()) return;
            drawPlot(g, plotArea.toFloat());

            const auto plot = plotArea.toFloat().reduced(15.0f, 19.0f);
            if (plot.getWidth() < 4 || plot.getHeight() < 4) return;

            const float a = static_cast<float>(knobs[0].slider.getValue());
            const float d = static_cast<float>(knobs[1].slider.getValue());
            const float sustain = static_cast<float>(knobs[2].slider.getValue());
            const float r = static_cast<float>(knobs[3].slider.getValue());

            // The sustain phase has no fixed duration. Display a representative hold.
            constexpr float hold = 0.65f;
            const float total = juce::jmax(0.02f, a + d + hold + r);
            const float x0 = plot.getX();
            const float x1 = x0 + plot.getWidth() * a / total;
            const float x2 = x0 + plot.getWidth() * (a + d) / total;
            const float x3 = x0 + plot.getWidth() * (a + d + hold) / total;
            const float x4 = plot.getRight();
            const float top = plot.getY() + 4.0f;
            const float bottom = plot.getBottom() - 4.0f;
            const float held = bottom - sustain * (bottom - top);

            juce::Path path;
            path.startNewSubPath(x0, bottom);
            path.lineTo(x1, top);
            path.lineTo(x2, held);
            path.lineTo(x3, held);
            path.lineTo(x4, bottom);

            juce::Path fill = path;
            fill.lineTo(x4, bottom);
            fill.lineTo(x0, bottom);
            fill.closeSubPath();
            g.setColour(cyan().withAlpha(0.10f));
            g.fillPath(fill);

            g.setColour(purple().withAlpha(0.35f));
            g.drawVerticalLine(static_cast<int>(x3), top, bottom);
            g.setColour(cyan());
            g.strokePath(path, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
            g.setColour(muted());
            g.setFont(10.0f);
            g.drawText("LEVEL", plotArea.getX() + 12, plotArea.getY() + 5,
                       80, 15, juce::Justification::centredLeft);
            g.drawText("NOTE OFF", plotArea.getRight() - 86, plotArea.getY() + 5,
                       74, 15, juce::Justification::centredRight);
        }

        void resized() override
        {
            // Wide envelope preview on the left; four consistent knobs on the right.
            auto body = getBodyBounds();
            plotArea = body.removeFromLeft(static_cast<int>(body.getWidth() * 0.55f));
            body.removeFromLeft(10);

            const int width = body.getWidth() / static_cast<int>(knobs.size());
            for (std::size_t i = 0; i < knobs.size(); ++i)
            {
                auto cell = i + 1 == knobs.size() ? body : body.removeFromLeft(width);
                positionKnob(knobs[i], cell);
            }
        }

    private:
        void timerCallback() override { repaint(plotArea); }

        std::array<Knob, 4> knobs;
        juce::Rectangle<int> plotArea;
    };
}

#pragma once

#include "Theme.h"
#include <array>
#include <cmath>

namespace osui
{
    // Per-note unison. Parameters are latched by SynthVoice at MIDI note-on.
    class UnisonPanel final : public ModulePanel, private juce::Timer
    {
    public:
        explicit UnisonPanel(APVTS& state)
            : ModulePanel("UNISON", "VOICE STACK")
        {
            setupKnob(*this, knobs[0], state, "unisonCount", "VOICES", cyan());
            setupKnob(*this, knobs[1], state, "unisonDetune", "DETUNE", purple(), " cents");
            setupKnob(*this, knobs[2], state, "unisonSpread", "SPREAD", orange());
            // Spread is 0..1; show it as a percentage of the full stereo width.
            knobs[2].slider.textFromValueFunction = [](double value)
            { return juce::String(juce::roundToInt(value * 100.0)) + " %"; };
            knobs[2].slider.valueFromTextFunction = [](const juce::String& text)
            { return juce::jlimit(0.0, 1.0, text.retainCharacters("0123456789.").getDoubleValue() / 100.0); };
            knobs[2].slider.updateText();
            // Sketch: display on the left, the knobs stacked on the right.
            for (auto& knob : knobs)
                makeKnobInline(knob, 86);
            setupPowerButton(*this, power, powerAttachment, state, "unisonEnabled");
            setPowerIndicator(power);
            knobs[0].slider.setTooltip("Oscillators per note (1-7). Applies to new notes.");
            knobs[1].slider.setTooltip("Detuning of the outer oscillators, in cents. Applies to new notes.");
            knobs[2].slider.setTooltip("Stereo width: the most detuned oscillators move furthest left/right. Applies to new notes.");
            startTimerHz(15);
        }

        void resized() override
        {
            power.setBounds(getWidth() - 70, 9, 55, 22);
            auto body = getBodyBounds();
            auto controls = body.removeFromRight(150);
            body.removeFromRight(12);
            plotArea = body;

            const int rowHeight = controls.getHeight() / static_cast<int>(knobs.size());
            for (std::size_t i = 0; i < knobs.size(); ++i)
            {
                auto row = i + 1 == knobs.size() ? controls : controls.removeFromTop(rowHeight);
                positionKnobInline(knobs[i], row, 46);
            }
        }

        void paint(juce::Graphics& g) override
        {
            ModulePanel::paint(g);
            if (plotArea.isEmpty()) return;
            drawPlot(g, plotArea.toFloat());
            const auto plot = plotArea.toFloat().reduced(22.0f, 22.0f);
            if (plot.isEmpty()) return;
            // Voice markers scale with the display height.
            const float tall = plot.getHeight() * 0.46f;
            const float shortLine = tall * 0.68f;

            const int count = juce::jlimit(1, 7, juce::roundToInt(knobs[0].slider.getValue()));
            const float cents = static_cast<float>(knobs[1].slider.getValue());
            const float spread = static_cast<float>(knobs[2].slider.getValue());
            const float amount = cents / 25.0f;
            const float centerX = plot.getCentreX();
            const float centreY = plot.getCentreY();

            g.setColour(muted().withAlpha(0.3f));
            g.drawVerticalLine(juce::roundToInt(centerX), plot.getY(), plot.getBottom());
            g.drawHorizontalLine(juce::roundToInt(centreY), plot.getX(), plot.getRight());

            for (int i = 0; i < count; ++i)
            {
                const float position = count == 1 ? 0.0f
                    : 2.0f * static_cast<float>(i) / static_cast<float>(count - 1) - 1.0f;
                const float x = centerX + position * amount * plot.getWidth() * 0.42f;
                const bool centre = std::abs(position) < 0.01f;
                g.setColour(centre ? cyan() : purple().withAlpha(0.8f));
                g.drawLine(x, centreY - (centre ? tall : shortLine),
                           x, centreY + (centre ? tall : shortLine), centre ? 3.2f : 2.4f);
                g.fillEllipse(x - 3.0f, centreY - 3.0f, 6.0f, 6.0f);
            }

            // Stereo strip along the top: where each oscillator sits L..R.
            const float stripY = static_cast<float>(plotArea.getY()) + 11.0f;
            const float halfWidth = plot.getWidth() * 0.42f;
            g.setColour(muted().withAlpha(0.45f));
            g.drawHorizontalLine(juce::roundToInt(stripY), centerX - halfWidth, centerX + halfWidth);
            g.setColour(muted());
            g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
            g.drawText("L", juce::Rectangle<float>(centerX - halfWidth - 14.0f, stripY - 6.0f, 10.0f, 12.0f),
                       juce::Justification::centred);
            g.drawText("R", juce::Rectangle<float>(centerX + halfWidth + 4.0f, stripY - 6.0f, 10.0f, 12.0f),
                       juce::Justification::centred);
            g.setColour(orange());
            for (int i = 0; i < count; ++i)
            {
                const float position = count == 1 ? 0.0f
                    : 2.0f * static_cast<float>(i) / static_cast<float>(count - 1) - 1.0f;
                const float x = centerX + position * spread * halfWidth;
                g.fillEllipse(x - 2.5f, stripY - 2.5f, 5.0f, 5.0f);
            }

            g.setColour(muted());
            g.setFont(juce::Font(juce::FontOptions(9.5f)));
            g.drawText("-" + juce::String(cents, 1) + " ct", plotArea.getX() + 8,
                       plotArea.getBottom() - 17, 70, 14, juce::Justification::centredLeft);
            g.drawText("+" + juce::String(cents, 1) + " ct", plotArea.getRight() - 78,
                       plotArea.getBottom() - 17, 70, 14, juce::Justification::centredRight);
        }

    private:
        void timerCallback() override { repaint(plotArea); }
        std::array<Knob, 3> knobs;
        PowerButton power;
        std::unique_ptr<ButtonAttachment> powerAttachment;
        juce::Rectangle<int> plotArea;
    };
}

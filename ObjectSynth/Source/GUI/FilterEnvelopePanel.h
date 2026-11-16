#pragma once

#include "Theme.h"
#include <array>
#include <cmath>

namespace osui
{
    class FilterEnvelopePanel final : public ModulePanel, private juce::Timer
    {
    public:
        explicit FilterEnvelopePanel(APVTS& state)
            : ModulePanel("FILTER ENVELOPE")
        {
            constexpr const char* ids[] = {
                "filterEnvAttack",
                "filterEnvDecay",
                "filterEnvSustain",
                "filterEnvRelease",
                "voiceFilterCutoff",
                "filterEnvAmount"
            };

            constexpr const char* names[] = {
                "ATTACK", "DECAY", "SUSTAIN",
                "RELEASE", "CUTOFF", "AMOUNT"
            };

            for (std::size_t i = 0; i < knobs.size(); ++i)
            {
                const juce::String suffix =
                    (i == 0 || i == 1 || i == 3) ? " s" :
                    (i == 4) ? " Hz" :
                    (i == 5) ? " oct" : "";

                setupKnob(*this, knobs[i], state,
                          ids[i], names[i], orange(), suffix);
                knobs[i].slider.setTextBoxStyle(
                    juce::Slider::TextBoxBelow, false, 70, 19);
            }

            setupPowerButton(*this, power, powerAttachment, state, "filterEnvEnabled");
            setPowerIndicator(power);
            startTimerHz(24);
        }

        void resized() override
        {
            power.setBounds(getWidth() - 70, 9, 55, 22);
            auto body = getBodyBounds();

            // Sketch: graph on the left, six knobs in a 3x2 grid on the right.
            plotArea = body.removeFromLeft(
                static_cast<int>(body.getWidth() * 0.46f)
            );

            body.removeFromLeft(14);

            // Six knobs in a 3x2 grid, same dial size as the other panels.
            const int rowHeight = body.getHeight() / 2;

            for (int row = 0; row < 2; ++row)
            {
                auto rowArea = row == 0
                    ? body.removeFromTop(rowHeight)
                    : body;

                const int cellWidth = rowArea.getWidth() / 3;

                for (int col = 0; col < 3; ++col)
                {
                    auto cell = col == 2
                        ? rowArea
                        : rowArea.removeFromLeft(cellWidth);

                    positionKnob(
                        knobs[static_cast<std::size_t>(row * 3 + col)],
                        cell.reduced(2, 0),
                        52
                    );
                }
            }
        }

        void paint(juce::Graphics& g) override
        {
            ModulePanel::paint(g);

            if (plotArea.isEmpty())
                return;

            drawPlot(g, plotArea.toFloat());

            const auto plot =
                plotArea.toFloat().reduced(15.0f, 20.0f);

            if (plot.getWidth() < 4.0f ||
                plot.getHeight() < 4.0f)
                return;

            const float attack =
                static_cast<float>(knobs[0].slider.getValue());

            const float decay =
                static_cast<float>(knobs[1].slider.getValue());

            const float sustain =
                static_cast<float>(knobs[2].slider.getValue());

            const float release =
                static_cast<float>(knobs[3].slider.getValue());

            const float baseCutoff =
                static_cast<float>(knobs[4].slider.getValue());

            const float amount =
                static_cast<float>(knobs[5].slider.getValue());

            // The sustain duration is illustrative.
            constexpr float sustainTime = 0.65f;

            const float total = juce::jmax(
                0.02f,
                attack + decay + sustainTime + release
            );

            const float x0 = plot.getX();
            const float x1 = x0 + plot.getWidth() * attack / total;
            const float x2 = x0 + plot.getWidth()
                                  * (attack + decay) / total;
            const float x3 = x0 + plot.getWidth()
                                  * (attack + decay + sustainTime) / total;
            const float x4 = plot.getRight();

            // Same logarithmic cutoff mapping as the DSP.
            auto cutoffAt = [baseCutoff, amount](float env)
            {
                return juce::jlimit(
                    20.0f, 18000.0f,
                    baseCutoff * std::exp2(amount * env)
                );
            };

            // Map a cutoff in Hz onto a logarithmic vertical axis.
            auto cutoffY = [plot](float hz)
            {
                const float normalized =
                    std::log(hz / 20.0f) /
                    std::log(18000.0f / 20.0f);

                return plot.getBottom() - 5.0f
                    - normalized * (plot.getHeight() - 10.0f);
            };

            const float yBase = cutoffY(cutoffAt(0.0f));
            const float yPeak = cutoffY(cutoffAt(1.0f));
            const float ySustain = cutoffY(cutoffAt(sustain));

            juce::Path curve;
            curve.startNewSubPath(x0, yBase);
            curve.lineTo(x1, yPeak);
            curve.lineTo(x2, ySustain);
            curve.lineTo(x3, ySustain);
            curve.lineTo(x4, yBase);

            // Reference line: base cutoff.
            g.setColour(muted().withAlpha(0.4f));

            juce::Path reference;
            reference.startNewSubPath(x0, yBase);
            reference.lineTo(x4, yBase);

            const float dashLengths[] = {4.0f, 4.0f};
            juce::Path dashed;
            juce::PathStrokeType(1.0f).createDashedStroke(
                dashed, reference, dashLengths, 2
            );
            g.fillPath(dashed);

            // Envelope curve.
            g.setColour(orange());
            g.strokePath(
                curve,
                juce::PathStrokeType(
                    2.5f,
                    juce::PathStrokeType::curved,
                    juce::PathStrokeType::rounded
                )
            );

            g.setColour(muted());
            g.setFont(10.0f);

            g.drawText(
                "CUTOFF (LOG)",
                plotArea.getX() + 12,
                plotArea.getY() + 5,
                110, 16,
                juce::Justification::centredLeft
            );

            g.drawText(
                "NOTE OFF",
                plotArea.getRight() - 85,
                plotArea.getBottom() - 21,
                75, 16,
                juce::Justification::centredRight
            );
        }

    private:
        void timerCallback() override
        {
            repaint(plotArea);
        }

        std::array<Knob, 6> knobs;
        PowerButton power;
        std::unique_ptr<ButtonAttachment> powerAttachment;
        juce::Rectangle<int> plotArea;
    };
}

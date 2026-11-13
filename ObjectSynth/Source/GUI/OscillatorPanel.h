#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"
#include <cmath>

namespace osui
{
    class OscillatorPanel final : public ModulePanel, private juce::Timer
    {
    public:
        explicit OscillatorPanel(ObjectSynthAudioProcessor& p)
            : ModulePanel("OSCILLATOR", "WAVETABLE"), processor(p)
        {
            const std::array<juce::String, 4> names {"SINE", "SAW", "SQUARE", "TRIANGLE"};
            for (std::size_t i = 0; i < buttons.size(); ++i)
            {
                auto& b = buttons[i];
                b.setButtonText(names[i]);
                b.setWantsKeyboardFocus(false);
                b.setMouseClickGrabsKeyboardFocus(false);
                b.onClick = [this, i]
                {
                    processor.setWaveform(static_cast<Waveform>(i));
                    refreshSelection();
                };
                addAndMakeVisible(b);
            }
            refreshSelection();
            startTimerHz(12);
        }

        void paint(juce::Graphics& g) override
        {
            ModulePanel::paint(g);
            if (plotArea.isEmpty())
                return;

            drawPlot(g, plotArea.toFloat());
            auto plot = plotArea.toFloat().reduced(14.0f, 20.0f);
            if (plot.getWidth() < 1 || plot.getHeight() < 1)
                return;

            g.setColour(muted().withAlpha(0.75f));
            g.setFont(10.5f);
            g.drawText("AMPLITUDE", plotArea.getX() + 13, plotArea.getY() + 7,
                       plotArea.getWidth() - 26, 15, juce::Justification::centredLeft);
            g.drawText("TWO CYCLES", plotArea.getX() + 13, plotArea.getBottom() - 22,
                       plotArea.getWidth() - 26, 14, juce::Justification::centredRight);

            juce::Path path;
            constexpr int samples = 260;
            for (int i = 0; i < samples; ++i)
            {
                const float phase = std::fmod(2.0f * static_cast<float>(i) / (samples - 1), 1.0f);
                float v = 0.0f;
                switch (processor.getWaveform())
                {
                    case Waveform::Sine:     v = std::sin(juce::MathConstants<float>::twoPi * phase); break;
                    case Waveform::Sawtooth: v = 2.0f * phase - 1.0f; break;
                    case Waveform::Square:   v = phase < 0.5f ? -1.0f : 1.0f; break;
                    case Waveform::Triangle: v = 1.0f - 4.0f * std::abs(phase - 0.5f); break;
                }
                const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / (samples - 1);
                const float y = plot.getCentreY() - v * plot.getHeight() * 0.40f;
                if (i == 0) path.startNewSubPath(x, y);
                else        path.lineTo(x, y);
            }
            g.setColour(cyan().withAlpha(0.14f));
            g.strokePath(path, juce::PathStrokeType(6.5f));
            g.setColour(cyan());
            g.strokePath(path, juce::PathStrokeType(2.1f));
        }

        void resized() override
        {
            auto area = getBodyBounds();
            auto row = area.removeFromBottom(37);
            row.reduce(1, 3);
            const auto cellWidth = row.getWidth() / static_cast<int>(buttons.size());
            for (std::size_t i = 0; i < buttons.size(); ++i)
            {
                auto cell = i + 1 == buttons.size() ? row : row.removeFromLeft(cellWidth);
                buttons[i].setBounds(cell.reduced(3, 0));
            }
            area.removeFromBottom(6);
            plotArea = area;
        }

    private:
        void timerCallback() override
        {
            if (processor.getWaveform() != lastWaveform)
                refreshSelection();
        }

        void refreshSelection()
        {
            const auto value = processor.getWaveform();
            lastWaveform = value;
            for (std::size_t i = 0; i < buttons.size(); ++i)
                buttons[i].setToggleState(static_cast<int>(i) == static_cast<int>(value),
                                          juce::dontSendNotification);
            repaint(plotArea);
        }

        ObjectSynthAudioProcessor& processor;
        std::array<juce::TextButton, 4> buttons;
        juce::Rectangle<int> plotArea;
        Waveform lastWaveform = Waveform::Sine;
    };
}

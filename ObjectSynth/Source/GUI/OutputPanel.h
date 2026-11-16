#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"
#include <array>
#include <cmath>

namespace osui
{
    // Stereo peak meter fed by ObjectSynthAudioProcessor::consumeOutputPeak().
    // Bars fall back smoothly; the CLIP light latches at 0 dBFS until clicked.
    class LevelMeter final : public juce::Component,
                             public juce::SettableTooltipClient,
                             private juce::Timer
    {
    public:
        explicit LevelMeter(ObjectSynthAudioProcessor& p) : processor(p)
        {
            setTooltip("Output peak level after Volume. Click to reset the CLIP light.");
            startTimerHz(30);
        }

        void paint(juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat();
            auto clipArea = area.removeFromRight(46.0f);
            area.removeFromRight(8.0f);
            auto scale = area.removeFromBottom(14.0f);

            const float barHeight = (area.getHeight() - 6.0f) / 2.0f;
            constexpr const char* names[] = {"L", "R"};
            for (std::size_t ch = 0; ch < 2; ++ch)
            {
                auto row = area.removeFromTop(barHeight);
                if (ch == 0)
                    area.removeFromTop(6.0f);

                g.setColour(muted());
                g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
                g.drawText(names[ch], row.removeFromLeft(16.0f), juce::Justification::centredLeft);

                g.setColour(plotFill());
                g.fillRoundedRectangle(row, 3.0f);
                const float fraction = toFraction(levels[ch]);
                auto fill = row.withWidth(row.getWidth() * fraction);
                juce::ColourGradient gradient(cyan(), row.getX(), 0.0f,
                                              orange(), row.getRight(), 0.0f, false);
                gradient.addColour(0.75, cyan());
                g.setGradientFill(gradient);
                g.fillRoundedRectangle(fill, 3.0f);

                const float holdX = row.getX() + row.getWidth() * toFraction(holds[ch]);
                if (holds[ch] > 0.0f)
                {
                    g.setColour(text().withAlpha(0.8f));
                    g.fillRect(holdX - 1.0f, row.getY(), 2.0f, row.getHeight());
                }
                g.setColour(panelEdge());
                g.drawRoundedRectangle(row, 3.0f, 1.0f);
                barX = row.getX();
                barWidth = row.getWidth();
            }

            // dBFS scale under the bars.
            g.setFont(juce::Font(juce::FontOptions(9.5f)));
            for (const int db : {-36, -24, -12, -6, 0})
            {
                const float x = barX + barWidth * toFraction(juce::Decibels::decibelsToGain(static_cast<float>(db)));
                g.setColour(muted());
                g.drawText(juce::String(db), juce::Rectangle<float>(x - 14.0f, scale.getY(), 28.0f, scale.getHeight()),
                           juce::Justification::centred);
            }

            // CLIP light.
            auto light = clipArea.withSizeKeepingCentre(clipArea.getWidth(), 22.0f);
            g.setColour(clipped ? juce::Colour::fromRGB(235, 70, 70) : plotFill());
            g.fillRoundedRectangle(light, 5.0f);
            g.setColour(clipped ? juce::Colours::white : muted());
            g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
            g.drawText("CLIP", light, juce::Justification::centred);
        }

        void mouseDown(const juce::MouseEvent&) override
        {
            clipped = false;
            holds = {};
            repaint();
        }

    private:
        static constexpr float floorDb = -48.0f;

        static float toFraction(float gain) noexcept
        {
            const float db = juce::Decibels::gainToDecibels(gain, floorDb);
            return juce::jlimit(0.0f, 1.0f, (db - floorDb) / -floorDb);
        }

        void timerCallback() override
        {
            bool changed = false;
            for (int ch = 0; ch < 2; ++ch)
            {
                const auto index = static_cast<std::size_t>(ch);
                const float peak = processor.consumeOutputPeak(ch);
                const float fallen = levels[index] * 0.82f; // about -26 dB per second
                const float level = juce::jmax(peak, fallen < 1.0e-4f ? 0.0f : fallen);
                if (peak >= 1.0f && ! clipped)
                    clipped = true;
                if (peak > holds[index] || ++holdAge[index] > 45) // ~1.5 s hold
                {
                    holds[index] = peak;
                    holdAge[index] = 0;
                }
                changed = changed || std::abs(level - levels[index]) > 1.0e-5f || peak > 0.0f;
                levels[index] = level;
            }
            if (changed || clipped)
                repaint();
        }

        ObjectSynthAudioProcessor& processor;
        std::array<float, 2> levels {}, holds {};
        std::array<int, 2> holdAge {};
        bool clipped = false;
        float barX = 0.0f, barWidth = 1.0f;
    };

    // OUTPUT: final Volume and stereo balance (Pan), plus the level meter.
    // The output stage is always active, so there is no ON/OFF switch.
    class OutputPanel final : public ModulePanel
    {
    public:
        explicit OutputPanel(ObjectSynthAudioProcessor& p)
            : ModulePanel("OUTPUT", "STEREO"), meter(p)
        {
            auto& state = p.getAPVTS();
            setupKnob(*this, volume, state, "outputVolume", "VOLUME", cyan(), " dB");
            setupKnob(*this, pan, state, "outputPan", "PAN", purple());

            // "L 40" / "C" / "R 40" reads better than -0.40 .. 0.40.
            pan.slider.textFromValueFunction = [](double value)
            {
                const int percent = juce::roundToInt(std::abs(value) * 100.0);
                return percent == 0 ? juce::String("C")
                                    : juce::String(value < 0.0 ? "L " : "R ") + juce::String(percent);
            };
            pan.slider.valueFromTextFunction = [](const juce::String& text)
            {
                const auto trimmed = text.trim().toUpperCase();
                const double amount = trimmed.retainCharacters("0123456789.").getDoubleValue() / 100.0;
                if (trimmed.startsWith("L")) return -amount;
                if (trimmed.startsWith("R")) return amount;
                return trimmed.getDoubleValue();
            };
            pan.slider.updateText();
            pan.slider.setDoubleClickReturnValue(true, 0.0);
            volume.slider.setDoubleClickReturnValue(true, 0.0);
            pan.slider.setTooltip("Output balance. Route the LFO to \"Pan\" for auto-pan.");
            pan.slider.getProperties().set(bipolarKnobProperty(), true);

            meterLabel.setText("LEVEL (dBFS)", juce::dontSendNotification);
            meterLabel.setFont(juce::Font(juce::FontOptions(10.5f)));
            meterLabel.setColour(juce::Label::textColourId, muted());
            addAndMakeVisible(meterLabel);
            addAndMakeVisible(meter);
        }

        void resized() override
        {
            auto area = getBodyBounds();
            auto knobs = area.removeFromTop(juce::jmin(110, area.getHeight() / 2));
            const int width = knobs.getWidth() / 2;
            positionKnob(volume, knobs.removeFromLeft(width), 56);
            positionKnob(pan, knobs, 56);

            area.removeFromTop(10);
            meterLabel.setBounds(area.removeFromTop(16));
            area.removeFromTop(4);
            meter.setBounds(area.withHeight(juce::jmin(area.getHeight(), 66)));
        }

    private:
        Knob volume, pan;
        juce::Label meterLabel;
        LevelMeter meter;
    };
}

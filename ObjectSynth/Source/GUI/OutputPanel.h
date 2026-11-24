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

        // Vertical, beside the Volume fader like a mixer channel strip: CLIP
        // light on top, L and R bars, dBFS scale on the right.
        void paint(juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat();
            auto light = area.removeFromTop(20.0f);
            area.removeFromTop(8.0f);
            auto names = area.removeFromBottom(14.0f);
            area.removeFromBottom(4.0f);
            auto scale = area.removeFromRight(24.0f);

            constexpr float barWidth = 9.0f, gap = 4.0f;
            const float left = area.getCentreX() - barWidth - gap * 0.5f;
            constexpr const char* channelNames[] = {"L", "R"};
            for (std::size_t ch = 0; ch < 2; ++ch)
            {
                const auto bar = juce::Rectangle<float>(left + static_cast<float>(ch) * (barWidth + gap),
                                                        area.getY(), barWidth, area.getHeight());
                g.setColour(juce::Colours::black);
                g.fillRoundedRectangle(bar.expanded(1.0f), 2.0f);
                g.setColour(plotFill());
                g.fillRoundedRectangle(bar, 2.0f);

                const auto fill = bar.withTrimmedTop(bar.getHeight() * (1.0f - toFraction(levels[ch])));
                juce::ColourGradient gradient(red(), 0.0f, bar.getY(), cyan(), 0.0f, bar.getBottom(), false);
                gradient.addColour(0.25, cyan());
                g.setGradientFill(gradient);
                g.fillRoundedRectangle(fill, 2.0f);

                if (holds[ch] > 0.0f)
                {
                    const float holdY = bar.getBottom() - bar.getHeight() * toFraction(holds[ch]);
                    g.setColour(text().withAlpha(0.85f));
                    g.fillRect(bar.getX(), holdY - 1.0f, bar.getWidth(), 2.0f);
                }

                g.setFont(font(10.0f));
                drawEngraved(g, channelNames[ch], bar.withY(names.getY()).withHeight(names.getHeight())
                                                      .expanded(4.0f, 0.0f).toNearestInt(),
                             juce::Justification::centred, muted());
                barTop = bar.getY();
                barHeight = bar.getHeight();
            }

            g.setFont(font(9.5f));
            for (const int db : {0, -12, -24, -36})
            {
                const float ty = barTop + barHeight * (1.0f - toFraction(juce::Decibels::decibelsToGain(static_cast<float>(db))));
                drawEngraved(g, juce::String(db), juce::Rectangle<float>(scale.getX() + 2.0f, ty - 6.0f,
                                                                         scale.getWidth() - 2.0f, 12.0f).toNearestInt(),
                             juce::Justification::centredLeft, muted());
            }

            // CLIP light: a red LED when the output hit 0 dBFS.
            light = light.withSizeKeepingCentre(juce::jmin(light.getWidth(), 46.0f), light.getHeight());
            g.setColour(clipped ? red() : plotFill());
            g.fillRoundedRectangle(light, 4.0f);
            g.setColour(juce::Colours::black);
            g.drawRoundedRectangle(light, 4.0f, 1.0f);
            g.setColour(clipped ? juce::Colours::white : muted());
            g.setFont(font(10.5f).withExtraKerningFactor(0.08f));
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
        float barTop = 0.0f, barHeight = 1.0f;
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
            // Volume is a fader, not a knob.
            volume.slider.setSliderStyle(juce::Slider::LinearVertical);
            volume.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
            volume.slider.getProperties().set(faderProperty(), true);
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

            meterLabel.setText("LEVEL", juce::dontSendNotification);
            meterLabel.setFont(juce::Font(juce::FontOptions(11.5f)));
            meterLabel.setJustificationType(juce::Justification::centred);
            meterLabel.setColour(juce::Label::textColourId, muted());
            addAndMakeVisible(meterLabel);
            addAndMakeVisible(meter);
        }

        void resized() override
        {
            // Channel strip: [Volume fader] [L/R meter] [Pan].
            auto area = getBodyBounds();
            auto faderColumn = area.removeFromLeft(juce::jmin(92, area.getWidth() / 3));
            area.removeFromLeft(4);
            auto meterColumn = area.removeFromLeft(juce::jmin(70, area.getWidth() / 2));
            area.removeFromLeft(4);

            volume.label.setBounds(faderColumn.removeFromTop(16));
            faderColumn.removeFromTop(4);
            volume.slider.setBounds(faderColumn);

            meterLabel.setBounds(meterColumn.removeFromTop(16));
            meterColumn.removeFromTop(4);
            meter.setBounds(meterColumn.withTrimmedBottom(volume.slider.getTextBoxHeight() - 14));

            positionKnob(pan, area.withSizeKeepingCentre(area.getWidth(), juce::jmin(area.getHeight(), 120)), 56);
        }

    private:
        Knob volume, pan;
        juce::Label meterLabel;
        LevelMeter meter;
    };
}

#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"
#include "../DSP/ModulationMatrix.h"
#include <array>
#include <cmath>

namespace osui
{
    class LFOPanel final : public ModulePanel, private juce::Timer
    {
    public:
        LFOPanel(APVTS& state, ObjectSynthAudioProcessor& processorToUse)
            : ModulePanel("LFO"), processor(processorToUse)
        {
            setupPowerButton(*this, power, powerAttachment, state, "lfoEnabled");
            setPowerIndicator(power);

            constexpr const char* waveformNames[] = {"Sine", "Saw", "Square", "Triangle"};
            for (int i = 0; i < 4; ++i)
                waveformChoice.addItem(waveformNames[i], i + 1);
            waveformChoice.setColour(juce::ComboBox::backgroundColourId, plotFill());
            waveformChoice.setColour(juce::ComboBox::textColourId, text());
            waveformChoice.setColour(juce::ComboBox::outlineColourId, panelEdge());
            waveformChoice.setMouseClickGrabsKeyboardFocus(false);
            addAndMakeVisible(waveformChoice);
            waveformAttachment = std::make_unique<ComboBoxAttachment>(
                state, "lfoWaveform", waveformChoice);

            // Tempo-synced rate: note divisions of the host tempo.
            for (std::size_t i = 0; i < TempoSync::divisions.size(); ++i)
                rateChoice.addItem(TempoSync::divisions[i].name, static_cast<int>(i + 1));
            rateChoice.setColour(juce::ComboBox::backgroundColourId, plotFill());
            rateChoice.setColour(juce::ComboBox::textColourId, text());
            rateChoice.setColour(juce::ComboBox::outlineColourId, panelEdge());
            rateChoice.setMouseClickGrabsKeyboardFocus(false);
            addAndMakeVisible(rateChoice);
            rateAttachment = std::make_unique<ComboBoxAttachment>(state, "lfoSync", rateChoice);

            rateLabel.setText("RATE", juce::dontSendNotification);
            rateLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
            rateLabel.setColour(juce::Label::textColourId, muted());
            rateLabel.setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(rateLabel);

            for (std::size_t i = 0; i < routeControls.size(); ++i)
            {
                auto& route = routeControls[i];
                for (std::size_t j = 0; j < modDestinationNames.size(); ++j)
                    route.destination.addItem(modDestinationNames[j], static_cast<int>(j + 1));

                route.destination.setColour(juce::ComboBox::backgroundColourId, plotFill());
                route.destination.setColour(juce::ComboBox::textColourId, text());
                route.destination.setColour(juce::ComboBox::outlineColourId, panelEdge());
                route.destination.setMouseClickGrabsKeyboardFocus(false);
                addAndMakeVisible(route.destination);

                route.depth.setSliderStyle(juce::Slider::LinearHorizontal);
                route.depth.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
                route.depth.setColour(juce::Slider::trackColourId, purple());
                route.depth.setMouseClickGrabsKeyboardFocus(false);
                addAndMakeVisible(route.depth);

                const auto number = juce::String(static_cast<int>(i + 1));
                route.destinationAttachment = std::make_unique<ComboBoxAttachment>(
                    state, juce::String("lfoRoute") + number + "Destination", route.destination);
                route.depthAttachment = std::make_unique<SliderAttachment>(
                    state, juce::String("lfoRoute") + number + "Depth", route.depth);
                // Display only: avoid "-0.00" for a centred depth. Set after the
                // attachment, which installs its own text function.
                route.depth.textFromValueFunction = [](double value)
                {
                    return juce::String(std::abs(value) < 0.005 ? 0.0 : value, 2);
                };
                route.depth.updateText();
            }

            startTimerHz(24);
        }

        void resized() override
        {
            power.setBounds(getWidth() - 70, 9, 55, 22);
            auto body = getBodyBounds();

            // Sketch, left column: display, waveform menu, rate slider.
            auto left = body.removeFromLeft(200);
            body.removeFromLeft(18);
            routesArea = body;

            auto rateRow = left.removeFromBottom(26);
            rateLabel.setBounds(rateRow.removeFromLeft(38));
            rateChoice.setBounds(rateRow);
            left.removeFromBottom(8);
            waveformChoice.setBounds(left.removeFromBottom(28));
            left.removeFromBottom(8);
            plotArea = left;

            // Sketch, right column: four rows of [destination menu] [depth slider].
            auto routesBody = routesArea;
            auto header = routesBody.removeFromTop(routeHeaderHeight);
            header.removeFromLeft(routeIndexWidth);
            depthHeader = header.removeFromRight(routeDepthWidth);
            header.removeFromRight(10);
            destinationHeader = header;

            const int rowHeight = routesBody.getHeight() / 4;
            for (std::size_t i = 0; i < routeControls.size(); ++i)
            {
                auto row = i + 1 == routeControls.size()
                    ? routesBody : routesBody.removeFromTop(rowHeight);
                routeRows[i] = row;
                row.removeFromLeft(routeIndexWidth);
                row = row.withSizeKeepingCentre(row.getWidth(), juce::jmin(28, row.getHeight()));
                auto depthBounds = row.removeFromRight(routeDepthWidth);
                row.removeFromRight(10);
                routeControls[i].destination.setBounds(row);
                routeControls[i].depth.setBounds(depthBounds);
            }
        }

        void paint(juce::Graphics& g) override
        {
            ModulePanel::paint(g);
            if (!plotArea.isEmpty())
            {
                drawPlot(g, plotArea.toFloat());
                auto chart = plotArea.toFloat().reduced(10.0f, 17.0f);
                if (chart.getWidth() > 2.0f && chart.getHeight() > 2.0f)
                {
                    const int waveform = juce::jlimit(0, 3, waveformChoice.getSelectedItemIndex());
                    juce::Path curve;
                    constexpr int steps = 160;
                    for (int i = 0; i <= steps; ++i)
                    {
                        const double phase = static_cast<double>(i) / steps;
                        const float y = chart.getCentreY() -
                            waveformAt(waveform, phase) * chart.getHeight() * 0.39f;
                        const float x = chart.getX() + chart.getWidth() * i / steps;
                        if (i == 0) curve.startNewSubPath(x, y);
                        else curve.lineTo(x, y);
                    }
                    g.setColour(cyan().withAlpha(0.85f));
                    g.strokePath(curve, juce::PathStrokeType(2.0f));

                    const float phase = juce::jlimit(0.0f, 1.0f,
                        processor.getLfoPhaseForUI());
                    const float x = chart.getX() + phase * chart.getWidth();
                    const float y = chart.getCentreY() -
                        waveformAt(waveform, phase) * chart.getHeight() * 0.39f;
                    g.setColour(orange());
                    g.fillEllipse(x - 3.8f, y - 3.8f, 7.6f, 7.6f);
                }
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.setColour(muted());
                g.drawText("ONE CYCLE", plotArea.reduced(9, 4).removeFromTop(14),
                           juce::Justification::centredLeft);
            }

            if (!routesArea.isEmpty())
            {
                g.setColour(muted());
                g.setFont(juce::Font(juce::FontOptions(11.0f)));
                g.drawText("DESTINATION", destinationHeader, juce::Justification::centredLeft);
                g.drawText("DEPTH", depthHeader, juce::Justification::centredLeft);

                g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
                for (std::size_t i = 0; i < routeRows.size(); ++i)
                {
                    const auto row = routeRows[i];
                    if (i > 0)
                    {
                        g.setColour(panelEdge().withAlpha(0.45f));
                        g.drawHorizontalLine(row.getY(), static_cast<float>(row.getX()),
                                             static_cast<float>(row.getRight()));
                    }
                    g.setColour(muted());
                    g.drawText(juce::String(static_cast<int>(i + 1)),
                               row.withWidth(routeIndexWidth - 4),
                               juce::Justification::centredLeft);
                }
            }
        }

    private:
        static float waveformAt(int type, double phase) noexcept
        {
            phase -= std::floor(phase);
            switch (type)
            {
                case 1: return static_cast<float>(2.0 * phase - 1.0);
                case 2: return phase < 0.5 ? 1.0f : -1.0f;
                case 3: return static_cast<float>(1.0 - 4.0 * std::abs(phase - 0.5));
                default: return static_cast<float>(
                    std::sin(juce::MathConstants<double>::twoPi * phase));
            }
        }

        void timerCallback() override { repaint(plotArea); }

        struct RouteControls
        {
            juce::ComboBox destination;
            juce::Slider depth;
            std::unique_ptr<ComboBoxAttachment> destinationAttachment;
            std::unique_ptr<SliderAttachment> depthAttachment;
        };

        ObjectSynthAudioProcessor& processor;
        PowerButton power;
        std::unique_ptr<ButtonAttachment> powerAttachment;
        juce::ComboBox waveformChoice;
        std::unique_ptr<ComboBoxAttachment> waveformAttachment;
        juce::ComboBox rateChoice;
        std::unique_ptr<ComboBoxAttachment> rateAttachment;
        juce::Label rateLabel;
        std::array<RouteControls, 4> routeControls;
        juce::Rectangle<int> plotArea;
        juce::Rectangle<int> routesArea;
        juce::Rectangle<int> destinationHeader, depthHeader;
        std::array<juce::Rectangle<int>, 4> routeRows;

        static constexpr int routeHeaderHeight = 18;
        static constexpr int routeIndexWidth = 20;
        static constexpr int routeDepthWidth = 150;
    };
}

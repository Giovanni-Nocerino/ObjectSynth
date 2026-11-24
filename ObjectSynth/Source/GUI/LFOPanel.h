#pragma once

#include "Theme.h"
#include "ModeDial.h"
#include "../PluginProcessor.h"
#include "../DSP/ModulationMatrix.h"
#include <array>
#include <cmath>
#include <functional>

namespace osui
{
    // Small left / right arrow on the LCD that steps a camera-menu line.
    class StepArrow final : public juce::Component
    {
    public:
        explicit StepArrow(int stepDirection) : direction(stepDirection)
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }

        std::function<void(int)> onStep;

        void paint(juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().withSizeKeepingCentre(7.0f, 10.0f);
            juce::Path arrow;
            if (direction < 0) arrow.addTriangle(r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX(), r.getCentreY());
            else               arrow.addTriangle(r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getCentreY());
            g.setColour((isMouseOver() ? juce::Colours::white : muted()).withMultipliedAlpha(isEnabled() ? 1.0f : 0.3f));
            g.fillPath(arrow);
        }

        void mouseEnter(const juce::MouseEvent&) override { repaint(); }
        void mouseExit(const juce::MouseEvent&) override { repaint(); }
        void mouseUp(const juce::MouseEvent& e) override
        {
            if (contains(e.getPosition()) && onStep) onStep(direction);
        }

    private:
        int direction;
    };

    class LFOPanel final : public ModulePanel, private juce::Timer
    {
    public:
        LFOPanel(APVTS& state, ObjectSynthAudioProcessor& processorToUse)
            : ModulePanel("LFO"), processor(processorToUse)
        {
            setupPowerButton(*this, power, powerAttachment, state, "lfoEnabled");
            setPowerIndicator(power);

            // Waveform and tempo-synced rate are camera dials, like the
            // oscillator's mode dial: the rate one reads like a shutter-speed dial.
            waveformDial.setTooltip("LFO waveform: turn the dial, click a mode or use the mouse wheel");
            addAndMakeVisible(waveformDial);
            waveformAttachment = std::make_unique<ModeDialAttachment>(state, "lfoWaveform", waveformDial);
            waveformAttachment->onChange = [this](int) { repaint(plotArea); };

            rateDial.setTooltip("LFO rate, synced to the host tempo: turn the dial, click a value "
                                "or use the mouse wheel");
            addAndMakeVisible(rateDial);
            rateAttachment = std::make_unique<ModeDialAttachment>(state, "lfoSync", rateDial);

            // Routes: lines of a camera menu on one LCD. The destination menu is
            // grouped by module; the depth is an exposure-compensation scale.
            for (std::size_t i = 0; i < routeControls.size(); ++i)
            {
                auto& route = routeControls[i];
                addDestinationItems(route.destination);
                route.destination.getProperties().set(lcdMenuProperty(), true);
                route.destination.setMouseClickGrabsKeyboardFocus(false);
                route.destination.setScrollWheelEnabled(true);
                route.destination.setTooltip("Destination: click for the menu, use the arrows or the mouse wheel");
                addAndMakeVisible(route.destination);

                for (auto* arrow : { &route.previous, &route.next })
                {
                    arrow->onStep = [this, i](int direction) { stepDestination(i, direction); };
                    addAndMakeVisible(*arrow);
                }

                route.depth.setSliderStyle(juce::Slider::LinearHorizontal);
                route.depth.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                route.depth.getProperties().set(evScaleProperty(), true);
                route.depth.setMouseClickGrabsKeyboardFocus(false);
                route.depth.setDoubleClickReturnValue(true, 0.0);
                route.depth.setTooltip("Depth: how far the LFO moves the destination, -1 .. +1 "
                                       "(double-click for 0)");
                route.depth.onValueChange = [this] { repaint(routesArea); };
                addAndMakeVisible(route.depth);

                const auto number = juce::String(static_cast<int>(i + 1));
                route.destinationAttachment = std::make_unique<ComboBoxAttachment>(
                    state, juce::String("lfoRoute") + number + "Destination", route.destination);
                route.depthAttachment = std::make_unique<SliderAttachment>(
                    state, juce::String("lfoRoute") + number + "Depth", route.depth);
                route.destination.onChange = [this, i] { refreshRoute(i); };
                refreshRoute(i);
            }

            startTimerHz(24);
        }

        void resized() override
        {
            power.setBounds(getWidth() - 70, 9, 55, 22);
            auto body = getBodyBounds();

            // Sketch, left column: display, waveform menu, rate slider.
            // Grows with the panel so a wide LFO does not just stretch the menus.
            auto left = body.removeFromLeft(juce::jlimit(200, 280,
                                                         juce::roundToInt(body.getWidth() * 0.36f)));
            body.removeFromLeft(18);
            routesArea = body;

            auto rateRow = left.removeFromBottom(dialHeight);
            rateLabelArea = rateRow.removeFromLeft(dialLabelWidth);
            rateDial.setBounds(rateRow);
            left.removeFromBottom(6);
            auto waveRow = left.removeFromBottom(dialHeight);
            waveLabelArea = waveRow.removeFromLeft(dialLabelWidth);
            waveformDial.setBounds(waveRow);
            left.removeFromBottom(8);
            plotArea = left;

            // Right column: one LCD with a header and four menu lines
            // [n] [DESTINATION      < >]  [-1 ... 0 ... +1] [value].
            auto screen = routesArea.reduced(10, 6);
            auto header = screen.removeFromTop(routeHeaderHeight);
            header.removeFromLeft(routeIndexWidth);
            routeDepthWidth = juce::jlimit(150, 230, juce::roundToInt(screen.getWidth() * 0.45f));
            valueHeader = header.removeFromRight(routeValueWidth);
            depthHeader = header.removeFromRight(routeDepthWidth);
            header.removeFromRight(14);
            destinationHeader = header;

            const int rowHeight = screen.getHeight() / 4;
            for (std::size_t i = 0; i < routeControls.size(); ++i)
            {
                auto row = i + 1 == routeControls.size() ? screen : screen.removeFromTop(rowHeight);
                routeRows[i] = row;
                row.removeFromLeft(routeIndexWidth);
                routeValues[i] = row.removeFromRight(routeValueWidth);
                routeControls[i].depth.setBounds(row.removeFromRight(routeDepthWidth));
                row.removeFromRight(14);
                auto line = row.withSizeKeepingCentre(row.getWidth(), juce::jmin(28, row.getHeight()));
                routeControls[i].next.setBounds(line.removeFromRight(18));
                routeControls[i].previous.setBounds(line.removeFromRight(18));
                line.removeFromRight(2);
                routeControls[i].destination.setBounds(line);
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
                    const int waveform = juce::jlimit(0, 4, waveformDial.getSelected());
                    const auto* custom = processor.getCustomWavetableForUI();
                    juce::Path curve;
                    constexpr int steps = 160;
                    for (int i = 0; i <= steps; ++i)
                    {
                        const double phase = static_cast<double>(i) / steps;
                        const float y = chart.getCentreY() -
                            waveformAt(waveform, phase, custom) * chart.getHeight() * 0.39f;
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
                        waveformAt(waveform, phase, custom) * chart.getHeight() * 0.39f;
                    g.setColour(red());
                    g.fillEllipse(x - 3.8f, y - 3.8f, 7.6f, 7.6f);
                }
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.setColour(muted());
                g.drawText("ONE CYCLE", plotArea.reduced(9, 4).removeFromTop(14),
                           juce::Justification::centredLeft);
            }

            // Engraved names beside the dials, level with the lettering
            // (below the dial's red index).
            g.setFont(font(11.0f).withExtraKerningFactor(0.08f));
            drawEngraved(g, "WAVE", waveLabelArea.withTrimmedTop(7), juce::Justification::centredLeft, muted());
            drawEngraved(g, "RATE", rateLabelArea.withTrimmedTop(7), juce::Justification::centredLeft, muted());

            if (!routesArea.isEmpty())
            {
                drawPlot(g, routesArea.toFloat(), false);

                g.setFont(font(10.0f).withExtraKerningFactor(0.08f));
                g.setColour(muted());
                g.drawText("DESTINATION", destinationHeader, juce::Justification::centredLeft);
                g.drawText("DEPTH", depthHeader, juce::Justification::centred);

                for (std::size_t i = 0; i < routeRows.size(); ++i)
                {
                    const auto row = routeRows[i];
                    const bool active = routeControls[i].destination.getSelectedItemIndex() > 0;
                    g.setColour(juce::Colours::white.withAlpha(0.06f));
                    g.drawHorizontalLine(row.getY(), static_cast<float>(row.getX()),
                                         static_cast<float>(row.getRight()));

                    // Route number; a red dot marks the routes that are in use.
                    const auto number = row.withWidth(routeIndexWidth);
                    g.setFont(font(13.0f));
                    g.setColour(active ? text() : muted().withAlpha(0.5f));
                    g.drawText(juce::String(static_cast<int>(i + 1)), number.withTrimmedLeft(8),
                               juce::Justification::centredLeft);
                    if (active)
                    {
                        g.setColour(red());
                        g.fillEllipse(juce::Rectangle<float>(4.0f, 4.0f)
                                          .withCentre({ number.getX() + 3.0f, number.toFloat().getCentreY() }));
                    }

                    const double depth = routeControls[i].depth.getValue();
                    const auto value = std::abs(depth) < 0.005 ? juce::String("0.00")
                                                               : (depth > 0 ? "+" : "") + juce::String(depth, 2);
                    g.setFont(font(13.0f));
                    g.setColour(active ? text() : muted().withAlpha(0.5f));
                    g.drawText(value, routeValues[i], juce::Justification::centredRight);
                }
            }
        }

    private:
        // Same shapes as DSP/LFO.h; Custom reads the oscillator's current frame.
        static float waveformAt(int type, double phase,
                                const WavetableBank::Table* custom = nullptr) noexcept
        {
            phase -= std::floor(phase);
            switch (type)
            {
                case 4:
                    if (custom != nullptr)
                    {
                        const double position = phase * static_cast<double>(custom->size());
                        const auto i = static_cast<std::size_t>(position) % custom->size();
                        const auto next = (i + 1) % custom->size();
                        const auto fraction = static_cast<float>(position - std::floor(position));
                        return (*custom)[i] + ((*custom)[next] - (*custom)[i]) * fraction;
                    }
                    return static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * phase));
                case 1: return static_cast<float>(2.0 * phase - 1.0);
                case 2: return phase < 0.5 ? 1.0f : -1.0f;
                case 3: return static_cast<float>(1.0 - 4.0 * std::abs(phase - 0.5));
                default: return static_cast<float>(
                    std::sin(juce::MathConstants<double>::twoPi * phase));
            }
        }

        void timerCallback() override
        {
            // CUSTOM needs a loaded wavetable (it reads the oscillator's frame).
            waveformDial.setModeEnabled(4, processor.hasCustomWavetable());
            repaint(plotArea);
        }

        // Destination menu grouped by module. Item IDs stay index + 1, in the
        // parameter's order, so the attachment's indices are unchanged.
        static void addDestinationItems(juce::ComboBox& box)
        {
            const auto add = [&box](ModDestination destination)
            {
                const auto index = static_cast<std::size_t>(destination);
                box.addItem(juce::String(modDestinationNames[index]).toUpperCase(), static_cast<int>(index) + 1);
            };
            add(ModDestination::None);
            const auto group = [&](const char* heading, ModDestination first, ModDestination last)
            {
                box.addSectionHeading(heading);
                for (int d = static_cast<int>(first); d <= static_cast<int>(last); ++d)
                    add(static_cast<ModDestination>(d));
            };
            group("FILTER BANK", ModDestination::LPCutoff, ModDestination::BPMix);
            group("VOICE", ModDestination::VoiceFilterCutoff, ModDestination::FilterEnvelopeAmount);
            group("DISTORTION", ModDestination::DistortionDrive, ModDestination::DistortionMix);
            group("CHORUS", ModDestination::ChorusRate, ModDestination::ChorusMix);
            group("OUTPUT", ModDestination::OutputPan, ModDestination::OutputPan);
        }

        void stepDestination(std::size_t route, int direction)
        {
            auto& box = routeControls[route].destination;
            box.setSelectedItemIndex(juce::jlimit(0, box.getNumItems() - 1,
                                                  box.getSelectedItemIndex() + direction));
        }

        // An unused route (NONE) is dimmed and its depth is locked.
        void refreshRoute(std::size_t route)
        {
            auto& controls = routeControls[route];
            const int selected = controls.destination.getSelectedItemIndex();
            const bool active = selected > 0;
            controls.destination.setColour(juce::ComboBox::textColourId,
                                           active ? text() : muted().withAlpha(0.6f));
            controls.depth.setEnabled(active);
            controls.previous.setEnabled(selected > 0);
            controls.next.setEnabled(selected < controls.destination.getNumItems() - 1);
            controls.previous.repaint();
            controls.next.repaint();
            repaint(routesArea);
        }

        struct RouteControls
        {
            juce::ComboBox destination;
            StepArrow previous { -1 }, next { 1 };
            juce::Slider depth;
            std::unique_ptr<ComboBoxAttachment> destinationAttachment;
            std::unique_ptr<SliderAttachment> depthAttachment;
        };

        ObjectSynthAudioProcessor& processor;
        PowerButton power;
        std::unique_ptr<ButtonAttachment> powerAttachment;
        static juce::StringArray rateNames()
        {
            juce::StringArray names;
            for (const auto& division : TempoSync::divisions)
                names.add(juce::String(division.name).toUpperCase());
            return names;
        }

        ModeDial waveformDial { { "SINE", "SAW", "SQUARE", "TRI", "CUSTOM" } };
        std::unique_ptr<ModeDialAttachment> waveformAttachment;
        ModeDial rateDial { rateNames() };
        std::unique_ptr<ModeDialAttachment> rateAttachment;
        juce::Rectangle<int> waveLabelArea, rateLabelArea;
        std::array<RouteControls, 4> routeControls;
        juce::Rectangle<int> plotArea;
        juce::Rectangle<int> routesArea;
        juce::Rectangle<int> destinationHeader, depthHeader, valueHeader;
        std::array<juce::Rectangle<int>, 4> routeRows, routeValues;

        static constexpr int dialHeight = 36;
        static constexpr int dialLabelWidth = 42;
        static constexpr int routeHeaderHeight = 18;
        static constexpr int routeIndexWidth = 26;
        static constexpr int routeValueWidth = 46;
        int routeDepthWidth = 150; // set in resized()
    };
}

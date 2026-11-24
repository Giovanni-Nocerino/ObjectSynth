#pragma once

#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <functional>

namespace osui
{
    // A camera mode dial seen edge-on: a knurled cylinder with the modes
    // engraved around its rim, turning under a fixed red index mark. Drag to
    // turn it (it snaps to the nearest mode), click a visible mode, or use the
    // mouse wheel. Disabled modes are skipped.
    class ModeDial final : public juce::Component, public juce::SettableTooltipClient,
                           private juce::Timer
    {
    public:
        explicit ModeDial(juce::StringArray modeNames) : names(std::move(modeNames))
        {
            enabled.resize(names.size(), true);
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        }

        std::function<void(int)> onChange;

        void setSelected(int index, bool animate = true)
        {
            index = juce::jlimit(0, names.size() - 1, index);
            selected = index;
            target = static_cast<float>(index);
            if (! animate || ! isShowing()) position = target;
            else startTimerHz(60);
            repaint();
        }

        int getSelected() const noexcept { return selected; }

        void setModeEnabled(int index, bool isEnabled)
        {
            if (juce::isPositiveAndBelow(index, names.size()) && enabled[(size_t) index] != isEnabled)
            {
                enabled[(size_t) index] = isEnabled;
                repaint();
            }
        }

        void paint(juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat();
            const auto body = r.withTrimmedTop(7.0f);
            const float cx = body.getCentreX();
            const float radius = body.getWidth() * 0.5f;

            // Cylinder: dark at the sides, lit in the middle, gloss-black.
            juce::Path shape;
            shape.addRoundedRectangle(body, 3.0f);
            juce::ColourGradient shade(juce::Colour::fromRGB(6, 7, 8), body.getX(), 0.0f,
                                       juce::Colour::fromRGB(6, 7, 8), body.getRight(), 0.0f, false);
            shade.addColour(0.5, juce::Colour::fromRGB(44, 46, 51));
            g.setGradientFill(shade);
            g.fillPath(shape);

            // Knurling: fine ridges along the lower rim, turning with the dial.
            const float knurlTop = body.getBottom() - body.getHeight() * 0.28f;
            for (int i = -40; i <= 40; ++i)
            {
                const float angle = (static_cast<float>(i) - position * 4.0f) * step / 4.0f;
                const float wrapped = std::remainder(angle, juce::MathConstants<float>::twoPi);
                if (std::abs(wrapped) >= juce::MathConstants<float>::halfPi) continue;
                const float x = cx + std::sin(wrapped) * radius;
                g.setColour(juce::Colour::fromRGB(80, 84, 90).withAlpha(0.15f + 0.6f * std::cos(wrapped)));
                g.drawVerticalLine(juce::roundToInt(x), knurlTop, body.getBottom() - 1.0f);
            }
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawHorizontalLine(juce::roundToInt(knurlTop), body.getX() + 2.0f, body.getRight() - 2.0f);

            // Engraved modes, foreshortened towards the sides.
            const auto labelBand = body.withBottom(knurlTop);
            const auto baseFont = font(juce::jmin(13.0f, labelBand.getHeight() * 0.62f))
                                      .withExtraKerningFactor(0.08f);
            for (int i = 0; i < names.size(); ++i)
            {
                const float angle = (static_cast<float>(i) - position) * step;
                if (std::abs(angle) >= juce::MathConstants<float>::halfPi * 0.98f) continue;
                const float squash = std::cos(angle);
                const float x = cx + std::sin(angle) * radius;
                const bool isSelected = i == selected && std::abs(position - target) < 0.02f;
                auto colour = isSelected ? juce::Colours::white
                                         : (enabled[(size_t) i] ? muted() : muted().withAlpha(0.35f));
                juce::Graphics::ScopedSaveState state(g);
                g.addTransform(juce::AffineTransform::scale(squash, 1.0f, x, labelBand.getCentreY()));
                g.setFont(baseFont);
                g.setColour(colour.withMultipliedAlpha(0.35f + 0.65f * squash));
                g.drawText(names[i], juce::Rectangle<float>(x - 60.0f, labelBand.getY(), 120.0f,
                                                            labelBand.getHeight()),
                           juce::Justification::centred, false);
            }

            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.drawHorizontalLine(juce::roundToInt(body.getY()) + 1, body.getX() + 3.0f, body.getRight() - 3.0f);
            g.setColour(juce::Colours::black);
            g.strokePath(shape, juce::PathStrokeType(1.0f));

            // Fixed red index mark above the centre.
            juce::Path index;
            index.addTriangle(cx - 5.0f, r.getY(), cx + 5.0f, r.getY(), cx, r.getY() + 6.0f);
            g.setColour(red());
            g.fillPath(index);
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            stopTimer();
            dragStart = position;
            dragged = false;
            (void) e;
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            const float radius = getWidth() * 0.5f;
            const float dx = static_cast<float>(e.getDistanceFromDragStartX());
            dragged = dragged || std::abs(dx) > 3.0f;
            position = juce::jlimit(-0.4f, static_cast<float>(names.size()) - 0.6f,
                                    dragStart - dx / (radius * step) * 1.6f);
            repaint();
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            if (! dragged)
            {
                // A click picks the mode drawn under the pointer.
                const float radius = getWidth() * 0.5f;
                const float s = juce::jlimit(-0.99f, 0.99f, (e.position.x - getWidth() * 0.5f) / radius);
                choose(juce::roundToInt(position + std::asin(s) / step));
                return;
            }
            choose(juce::roundToInt(position));
        }

        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
        {
            const float delta = std::abs(wheel.deltaX) > std::abs(wheel.deltaY) ? -wheel.deltaX : wheel.deltaY;
            if (delta != 0.0f)
                choose(selected + (delta < 0.0f ? 1 : -1), delta < 0.0f ? 1 : -1);
        }

    private:
        // Snaps to the nearest enabled mode (searching in `direction` first).
        void choose(int index, int direction = 0)
        {
            index = juce::jlimit(0, names.size() - 1, index);
            if (! enabled[(size_t) index])
            {
                const int first = direction != 0 ? direction : -1;
                for (int d = 1; d < names.size(); ++d)
                {
                    for (int sign : { first, -first })
                    {
                        const int candidate = index + sign * d;
                        if (juce::isPositiveAndBelow(candidate, names.size()) && enabled[(size_t) candidate])
                        {
                            index = candidate;
                            d = names.size();
                            break;
                        }
                    }
                }
            }
            const bool changed = index != selected;
            setSelected(index);
            if (changed && onChange) onChange(index);
        }

        void timerCallback() override
        {
            position += (target - position) * 0.25f;
            if (std::abs(target - position) < 0.005f)
            {
                position = target;
                stopTimer();
            }
            repaint();
        }

        juce::StringArray names;
        std::vector<bool> enabled;
        static constexpr float step = 0.62f;  // radians between engraved modes
        float position = 0.0f, target = 0.0f, dragStart = 0.0f;
        int selected = 0;
        bool dragged = false;
    };

    // Keeps a ModeDial and a choice parameter (index = mode) in sync, like a
    // ComboBoxAttachment does for a ComboBox.
    class ModeDialAttachment
    {
    public:
        ModeDialAttachment(juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                           ModeDial& dialToUse)
            : dial(dialToUse),
              attachment(*state.getParameter(parameterID),
                         [this](float value) { dial.setSelected(juce::roundToInt(value)); })
        {
            dial.onChange = [this](int index)
            {
                attachment.setValueAsCompleteGesture(static_cast<float>(index));
                if (onChange) onChange(index);
            };
            attachment.sendInitialUpdate();
        }

        ~ModeDialAttachment() { dial.onChange = nullptr; }

        std::function<void(int)> onChange;  // after the parameter is set

    private:
        ModeDial& dial;
        juce::ParameterAttachment attachment;
    };
}

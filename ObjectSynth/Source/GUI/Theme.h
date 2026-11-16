#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <memory>

namespace osui
{
    inline juce::Colour background() { return juce::Colour::fromRGB(12, 18, 29); }
    inline juce::Colour panel()      { return juce::Colour::fromRGB(22, 31, 46); }
    inline juce::Colour panelEdge()  { return juce::Colour::fromRGB(42, 55, 73); }
    inline juce::Colour plotFill()   { return juce::Colour::fromRGB(16, 25, 38); }
    inline juce::Colour text()       { return juce::Colour::fromRGB(239, 245, 250); }
    inline juce::Colour muted()      { return juce::Colour::fromRGB(149, 167, 184); }
    inline juce::Colour cyan()       { return juce::Colour::fromRGB(80, 200, 245); }
    inline juce::Colour orange()     { return juce::Colour::fromRGB(252, 180, 98); }
    inline juce::Colour purple()     { return juce::Colour::fromRGB(177, 146, 255); }

    class LookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour(juce::Slider::textBoxTextColourId, text());
            setColour(juce::Slider::textBoxBackgroundColourId, plotFill());
            setColour(juce::Slider::textBoxOutlineColourId, panelEdge());
            setColour(juce::Label::textColourId, text());
            setColour(juce::TextButton::buttonColourId, panelEdge());
            setColour(juce::TextButton::buttonOnColourId, cyan().darker(0.45f));
            setColour(juce::TextButton::textColourOffId, text());
            setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        }

        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float position, float startAngle, float endAngle,
                              juce::Slider& slider) override
        {
            const float cx = static_cast<float>(x + width / 2);
            const float cy = static_cast<float>(y + height / 2);
            const float radius = juce::jmin(static_cast<float>(width), static_cast<float>(height)) * 0.37f;
            if (radius < 7.0f)
                return;

            const float angle = startAngle + position * (endAngle - startAngle);
            auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);

            juce::Path track;
            track.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
            g.setColour(panelEdge().brighter(0.25f));
            g.strokePath(track, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

            juce::Path value;
            value.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle,
                                juce::jmax(startAngle + 0.001f, angle), true);
            g.setColour(accent);
            g.strokePath(value, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

            g.setColour(juce::Colour::fromRGB(34, 48, 66));
            g.fillEllipse(cx - radius * 0.72f, cy - radius * 0.72f,
                          radius * 1.44f, radius * 1.44f);
            g.setColour(panelEdge().brighter(0.55f));
            g.drawEllipse(cx - radius * 0.72f, cy - radius * 0.72f,
                          radius * 1.44f, radius * 1.44f, 1.0f);

            const auto needle = juce::Point<float>(cx + std::sin(angle) * radius * 0.60f,
                                                   cy - std::cos(angle) * radius * 0.60f);
            g.setColour(text());
            g.drawLine(cx, cy, needle.x, needle.y, 2.8f);
        }
    };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using APVTS = juce::AudioProcessorValueTreeState;
    using ButtonAttachment = APVTS::ButtonAttachment;
    using ComboBoxAttachment = APVTS::ComboBoxAttachment;

    // A small, clearly legible bypass switch for each module header.
    class PowerButton final : public juce::TextButton
    {
    public:
        PowerButton()
        {
            setClickingTogglesState(true);
            setMouseClickGrabsKeyboardFocus(false);
            setWantsKeyboardFocus(false);
        }

        void paintButton(juce::Graphics& g, bool isOver, bool isDown) override
        {
            const bool active = getToggleState();
            const auto r = getLocalBounds().toFloat().reduced(0.5f);
            const auto accent = active ? cyan() : muted();
            g.setColour(active ? cyan().withAlpha(isDown ? 0.42f : 0.22f)
                               : panelEdge().withAlpha(isOver ? 0.92f : 0.65f));
            g.fillRoundedRectangle(r, 6.0f);
            g.setColour(accent.withAlpha(0.75f));
            g.drawRoundedRectangle(r, 6.0f, 1.0f);
            g.setColour(active ? osui::text() : osui::muted());
            g.setFont(juce::Font(juce::FontOptions(10.5f)));
            g.drawFittedText(active ? "ON" : "OFF", getLocalBounds(),
                             juce::Justification::centred, 1);
        }
    };

    inline void setupPowerButton(juce::Component& owner, PowerButton& button,
                                 std::unique_ptr<ButtonAttachment>& attachment,
                                 APVTS& state, const juce::String& parameterID)
    {
        owner.addAndMakeVisible(button);
        attachment = std::make_unique<ButtonAttachment>(state, parameterID, button);
    }

    // The attachment is declared last so it is destroyed before its slider.
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    inline void setupKnob(juce::Component& owner, Knob& control, APVTS& apvts,
                          const juce::String& id, const juce::String& caption,
                          juce::Colour accent, const juce::String& suffix = {})
    {
        control.label.setText(caption, juce::dontSendNotification);
        control.label.setJustificationType(juce::Justification::centred);
        control.label.setColour(juce::Label::textColourId, muted());
        control.label.setFont(juce::Font(juce::FontOptions(11.5f)));
        owner.addAndMakeVisible(control.label);

        control.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        control.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 86, 20);
        control.slider.setMouseClickGrabsKeyboardFocus(false);
        control.slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        control.slider.setColour(juce::Slider::textBoxTextColourId, text());
        if (suffix.isNotEmpty())
            control.slider.setTextValueSuffix(suffix);
        owner.addAndMakeVisible(control.slider);
        control.attachment = std::make_unique<SliderAttachment>(apvts, id, control.slider);
    }

    inline void positionKnob(Knob& knob, juce::Rectangle<int> area)
    {
        knob.label.setBounds(area.removeFromTop(17));
        knob.slider.setBounds(area);
    }

    inline void drawPlot(juce::Graphics& g, juce::Rectangle<float> rect)
    {
        g.setColour(plotFill());
        g.fillRoundedRectangle(rect, 9.0f);
        g.setColour(panelEdge().withAlpha(0.55f));
        g.drawRoundedRectangle(rect, 9.0f, 1.0f);

        auto inner = rect.reduced(8.0f);
        g.setColour(panelEdge().withAlpha(0.40f));
        for (int i = 1; i < 4; ++i)
        {
            const auto x = inner.getX() + inner.getWidth() * i / 4.0f;
            const auto y = inner.getY() + inner.getHeight() * i / 4.0f;
            g.drawVerticalLine(static_cast<int>(x), inner.getY(), inner.getBottom());
            g.drawHorizontalLine(static_cast<int>(y), inner.getX(), inner.getRight());
        }
    }

    class ModulePanel : public juce::Component
    {
    public:
        explicit ModulePanel(juce::String title, juce::String eyebrow = {})
            : heading(std::move(title)), detail(std::move(eyebrow)) {}

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(0.8f);
            g.setColour(panel());
            g.fillRoundedRectangle(bounds, 13.0f);
            g.setColour(panelEdge());
            g.drawRoundedRectangle(bounds, 13.0f, 1.1f);

            g.setColour(text());
            g.setFont(15.0f);
            g.drawText(heading, 16, 10, getWidth() - 32, 21, juce::Justification::centredLeft);
            if (detail.isNotEmpty())
            {
                g.setColour(muted());
                g.setFont(10.5f);
                g.drawText(detail, 16, 12, getWidth() - 32, 18,
                           juce::Justification::centredRight);
            }
        }

    protected:
        juce::Rectangle<int> getBodyBounds() const
        {
            auto bounds = getLocalBounds().reduced(12);
            bounds.removeFromTop(29);
            return bounds;
        }

    private:
        juce::String heading, detail;
    };
}

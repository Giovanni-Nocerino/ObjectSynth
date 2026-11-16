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

    // Height of the caption drawn above an inline knob's value box.
    constexpr int inlineCaptionHeight = 15;

    // Marks a rotary slider whose value box sits to the right of the dial,
    // with its caption above the box (see positionKnobInline()).
    inline const juce::Identifier& inlineKnobProperty()
    {
        static const juce::Identifier id { "osInlineKnob" };
        return id;
    }

    // APVTS parameter ID a knob is attached to (set by setupKnob()).
    inline const juce::Identifier& parameterIdProperty()
    {
        static const juce::Identifier id { "osParameterId" };
        return id;
    }

    // Live modulated position of a knob (0..1 proportion of its range), set by
    // the editor's modulation timer. Absent when the parameter is not modulated.
    inline const juce::Identifier& modulatedPositionProperty()
    {
        static const juce::Identifier id { "osModulatedPosition" };
        return id;
    }

    inline juce::Colour modulation() { return juce::Colour::fromRGB(255, 214, 102); }

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
            const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
            const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
            const float radius = juce::jmin(static_cast<float>(width), static_cast<float>(height)) * 0.41f;
            if (radius < 6.0f)
                return;

            const float angle = startAngle + position * (endAngle - startAngle);
            auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
            // Arc thickness follows the dial size so small and large knobs match.
            const float stroke = juce::jlimit(2.5f, 4.0f, radius * 0.17f);

            juce::Path track;
            track.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
            g.setColour(panelEdge().brighter(0.25f));
            g.strokePath(track, juce::PathStrokeType(stroke, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

            juce::Path value;
            value.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle,
                                juce::jmax(startAngle + 0.001f, angle), true);
            g.setColour(accent);
            g.strokePath(value, juce::PathStrokeType(stroke, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

            // Live modulation: an outer arc from the set value to where the LFO
            // currently pushes it, plus a dot at the modulated position.
            const auto& modulated = slider.getProperties()[modulatedPositionProperty()];
            if (! modulated.isVoid())
            {
                const float modPosition = juce::jlimit(0.0f, 1.0f, static_cast<float>(static_cast<double>(modulated)));
                const float modAngle = startAngle + modPosition * (endAngle - startAngle);
                const float outer = radius + stroke + 1.5f;

                juce::Path modArc;
                modArc.addCentredArc(cx, cy, outer, outer, 0.0f,
                                     juce::jmin(angle, modAngle), juce::jmax(angle, modAngle) + 0.001f, true);
                g.setColour(modulation().withAlpha(0.85f));
                g.strokePath(modArc, juce::PathStrokeType(juce::jmax(1.5f, stroke * 0.6f),
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));

                const float dot = juce::jmax(2.5f, stroke * 0.8f);
                g.setColour(modulation());
                g.fillEllipse(cx + std::sin(modAngle) * outer - dot,
                              cy - std::cos(modAngle) * outer - dot, dot * 2.0f, dot * 2.0f);
            }

            g.setColour(juce::Colour::fromRGB(34, 48, 66));
            g.fillEllipse(cx - radius * 0.72f, cy - radius * 0.72f,
                          radius * 1.44f, radius * 1.44f);
            g.setColour(panelEdge().brighter(0.55f));
            g.drawEllipse(cx - radius * 0.72f, cy - radius * 0.72f,
                          radius * 1.44f, radius * 1.44f, 1.0f);

            const auto needle = juce::Point<float>(cx + std::sin(angle) * radius * 0.60f,
                                                   cy - std::cos(angle) * radius * 0.60f);
            g.setColour(text());
            g.drawLine(cx, cy, needle.x, needle.y, juce::jlimit(2.0f, 2.8f, radius * 0.12f));
        }

        // Inline knobs: dial on the left, caption + value box stacked on the right.
        juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override
        {
            if (! static_cast<bool>(slider.getProperties()[inlineKnobProperty()]))
                return juce::LookAndFeel_V4::getSliderLayout(slider);

            const auto bounds = slider.getLocalBounds();
            const int boxWidth = juce::jmin(slider.getTextBoxWidth(), bounds.getWidth() / 2);
            const int boxHeight = juce::jmin(slider.getTextBoxHeight(), bounds.getHeight());
            const int groupTop = (bounds.getHeight() - (inlineCaptionHeight + boxHeight)) / 2;

            juce::Slider::SliderLayout layout;
            layout.textBoxBounds = { bounds.getWidth() - boxWidth,
                                     juce::jmax(0, groupTop + inlineCaptionHeight),
                                     boxWidth, boxHeight };
            layout.sliderBounds = bounds.withTrimmedRight(boxWidth + 6);
            return layout;
        }

        // Fixed, legible menu text: JUCE otherwise scales it with the box height.
        juce::Font getComboBoxFont(juce::ComboBox&) override
        {
            return juce::Font(juce::FontOptions(13.0f));
        }

        juce::Font getPopupMenuFont() override
        {
            return juce::Font(juce::FontOptions(14.0f));
        }

        juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
        {
            return juce::Font(juce::FontOptions(juce::jmin(14.0f, static_cast<float>(buttonHeight) * 0.5f)));
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
        control.slider.getProperties().set(parameterIdProperty(), id);
        control.attachment = std::make_unique<SliderAttachment>(apvts, id, control.slider);
    }

    // Classic stacked knob: caption, dial, value box. The dial is capped at
    // maxDial so knobs keep a consistent size across panels of different
    // heights; the group is centred vertically inside its cell.
    inline void positionKnob(Knob& knob, juce::Rectangle<int> area, int maxDial = 56)
    {
        const bool hasCaption = knob.label.getText().isNotEmpty();
        const int captionHeight = hasCaption ? 16 : 0;
        const int boxHeight = knob.slider.getTextBoxHeight();
        const int dial = juce::jmax(0, juce::jmin(maxDial, area.getWidth(),
                                                  area.getHeight() - captionHeight - boxHeight - 2));
        auto stack = area.withSizeKeepingCentre(area.getWidth(),
                                                captionHeight + dial + 2 + boxHeight);
        knob.label.setBounds(hasCaption ? stack.removeFromTop(captionHeight)
                                        : juce::Rectangle<int>());
        knob.slider.setBounds(stack);
    }

    // Switches a knob to the compact inline style used in narrow panels.
    inline void makeKnobInline(Knob& knob, int boxWidth, int boxHeight = 20)
    {
        knob.slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, boxWidth, boxHeight);
        knob.slider.getProperties().set(inlineKnobProperty(), true);
        knob.label.setFont(juce::Font(juce::FontOptions(10.5f)));
    }

    // Inline knob: [dial] [CAPTION / value]. The caption is a sibling label
    // placed exactly above the value box computed by LookAndFeel::getSliderLayout.
    inline void positionKnobInline(Knob& knob, juce::Rectangle<int> area, int maxDial = 50)
    {
        const int boxWidth = knob.slider.getTextBoxWidth();
        const int boxHeight = knob.slider.getTextBoxHeight();
        const int dial = juce::jmax(0, juce::jmin(maxDial, area.getHeight(),
                                                  area.getWidth() - boxWidth - 6));
        const int width = juce::jmin(area.getWidth(), dial + 6 + boxWidth);
        const int height = juce::jmin(area.getHeight(),
                                      juce::jmax(dial, inlineCaptionHeight + boxHeight));
        const auto group = area.withSizeKeepingCentre(width, height);
        knob.slider.setBounds(group);

        const int captionTop = group.getY()
            + (group.getHeight() - (inlineCaptionHeight + boxHeight)) / 2;
        knob.label.setBounds(group.getRight() - juce::jmin(boxWidth, group.getWidth() / 2),
                             captionTop, juce::jmin(boxWidth, group.getWidth() / 2),
                             inlineCaptionHeight);
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
            // The upper-right header is reserved for the power switch.
            if (detail.isNotEmpty() && powerIndicator == nullptr)
            {
                g.setColour(muted());
                g.setFont(10.5f);
                g.drawText(detail, 16, 12, getWidth() - 32, 18,
                           juce::Justification::centredRight);
            }
        }

    protected:
        // Only visual: the audio bypass is controlled by the matching APVTS
        // bool parameter. The button remains clickable while the module is off.
        void setPowerIndicator(PowerButton& button)
        {
            powerIndicator = &button;
            button.onStateChange = [this] { repaint(); };
        }

        void paintOverChildren(juce::Graphics& g) override
        {
            if (powerIndicator == nullptr || powerIndicator->getToggleState())
                return;

            // Cover just the module body; keep the ON/OFF switch fully legible.
            auto body = getLocalBounds().reduced(2).withTrimmedTop(38);
            if (body.isEmpty())
                return;

            g.setColour(background().withAlpha(0.57f));
            g.fillRoundedRectangle(body.toFloat(), 9.0f);
        }

        juce::Rectangle<int> getBodyBounds() const
        {
            auto bounds = getLocalBounds().reduced(12);
            bounds.removeFromTop(29);
            return bounds;
        }

    private:
        juce::String heading, detail;
        PowerButton* powerIndicator = nullptr; // non-owning; member of panel
    };
}

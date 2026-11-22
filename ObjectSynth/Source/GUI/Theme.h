#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <memory>

namespace osui
{
    // Camera look: space-grey anodised body, gloss-black dials and screens,
    // engraved DIN lettering, and a few red details (status LEDs, shutter).
    inline juce::Colour background() { return juce::Colour::fromRGB(30, 32, 35); }   // body
    inline juce::Colour panel()      { return juce::Colour::fromRGB(43, 45, 49); }   // top plates
    inline juce::Colour panelEdge()  { return juce::Colour::fromRGB(66, 69, 74); }
    inline juce::Colour plotFill()   { return juce::Colour::fromRGB(12, 13, 15); }   // gloss black
    inline juce::Colour text()       { return juce::Colour::fromRGB(236, 235, 231); }
    inline juce::Colour muted()      { return juce::Colour::fromRGB(160, 164, 170); }
    // Value colours (names kept from the original theme).
    inline juce::Colour cyan()       { return juce::Colour::fromRGB(233, 230, 222); } // ivory
    inline juce::Colour orange()     { return juce::Colour::fromRGB(226, 161, 74); }  // brass
    inline juce::Colour purple()     { return juce::Colour::fromRGB(168, 136, 245); } // lens-coating violet
    inline juce::Colour green()      { return juce::Colour::fromRGB(112, 214, 162); } // lens-coating green
    inline juce::Colour red()        { return juce::Colour::fromRGB(226, 38, 30); }   // accent, sparingly
    // Anti-reflective coating tints of the lens, echoed on every screen.
    inline juce::Colour coatingViolet() { return juce::Colour::fromRGB(150, 110, 230); }
    inline juce::Colour coatingGreen()  { return juce::Colour::fromRGB(90, 200, 150); }
    inline juce::Colour glossTop()   { return juce::Colour::fromRGB(48, 50, 55); }
    inline juce::Colour glossBottom(){ return juce::Colour::fromRGB(14, 15, 17); }
    inline juce::Colour recess()     { return juce::Colour::fromRGB(20, 21, 24); }
    inline juce::Colour marble()     { return juce::Colour::fromRGB(236, 234, 229); } // warm stone white
    inline juce::Colour ink()        { return juce::Colour::fromRGB(24, 25, 28); }    // text on marble

    // DIN ships with macOS (Supplemental fonts); elsewhere JUCE's default sans is used.
    inline juce::Typeface::Ptr loadSystemTypeface(const char* path)
    {
        juce::MemoryBlock data;
        if (! juce::File(path).loadFileAsData(data) || data.isEmpty())
            return nullptr;
        return juce::Typeface::createSystemTypefaceFor(data.getData(), data.getSize());
    }

    inline juce::Typeface::Ptr bodyTypeface()
    {
        static const auto typeface = loadSystemTypeface(
            "/System/Library/Fonts/Supplemental/DIN Alternate Bold.ttf");
        return typeface;
    }

    inline juce::Typeface::Ptr titleTypeface()
    {
        static const auto typeface = loadSystemTypeface(
            "/System/Library/Fonts/Supplemental/DIN Condensed Bold.ttf");
        return typeface;
    }

    inline juce::Font font(float height)
    {
        if (auto typeface = bodyTypeface())
            return juce::Font(juce::FontOptions(typeface).withHeight(height));
        return juce::Font(juce::FontOptions(height));
    }

    // Condensed, letter-spaced, for engraved module titles and the logo.
    inline juce::Font titleFont(float height, float tracking = 0.08f)
    {
        auto options = titleTypeface() != nullptr ? juce::FontOptions(titleTypeface())
                                                  : juce::FontOptions();
        return juce::Font(options.withHeight(height).withKerningFactor(tracking));
    }

    // Polished white stone: warm base, soft shading and a few faint grey
    // veins. The veins come from a fixed seed, so every repaint matches.
    inline void fillMarble(juce::Graphics& g, juce::Rectangle<float> r, int seed = 7,
                           float cornerSize = 0.0f)
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(r, cornerSize);
        g.reduceClipRegion(clip);

        g.setGradientFill(juce::ColourGradient(marble().brighter(0.04f), r.getX(), r.getY(),
                                               marble().darker(0.07f), r.getRight(), r.getBottom(), false));
        g.fillRect(r);

        juce::Random random(seed);
        const int veins = juce::jmax(2, juce::roundToInt(r.getWidth() * r.getHeight() / 9000.0f));
        for (int v = 0; v < juce::jmin(veins, 14); ++v)
        {
            juce::Path vein;
            float x = r.getX() + random.nextFloat() * r.getWidth();
            float y = r.getY() - 4.0f;
            vein.startNewSubPath(x, y);
            const float drift = (random.nextFloat() - 0.3f) * juce::jmax(r.getWidth(), r.getHeight()) * 0.9f;
            const int steps = 5;
            for (int i = 1; i <= steps; ++i)
            {
                const float t = static_cast<float>(i) / steps;
                const float nx = x + drift * t + (random.nextFloat() - 0.5f) * 14.0f;
                const float ny = r.getY() - 4.0f + (r.getHeight() + 8.0f) * t;
                vein.quadraticTo(x + (random.nextFloat() - 0.5f) * 18.0f, (y + ny) * 0.5f, nx, ny);
                x = nx; y = ny;
            }
            const float width = 0.4f + random.nextFloat() * 0.9f;
            g.setColour(juce::Colour::fromRGB(150, 150, 152).withAlpha(0.05f + random.nextFloat() * 0.07f));
            g.strokePath(vein, juce::PathStrokeType(width * 3.0f, juce::PathStrokeType::curved));
            g.setColour(juce::Colour::fromRGB(128, 129, 132).withAlpha(0.10f + random.nextFloat() * 0.14f));
            g.strokePath(vein, juce::PathStrokeType(width, juce::PathStrokeType::curved));
        }

        // Satin sheen.
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.35f), r.getX(), r.getY(),
                                               juce::Colours::white.withAlpha(0.0f), r.getX(),
                                               r.getY() + r.getHeight() * 0.5f, false));
        g.fillRect(r);
    }

    // Pebbled leatherette, like the covering of a camera body: a seamless
    // tile of small raised grains, built once and drawn tiled.
    inline const juce::Image& leatheretteTile()
    {
        static const juce::Image tile = []
        {
            constexpr int size = 192;
            juce::Image image(juce::Image::RGB, size, size, false);
            juce::Graphics g(image);
            g.fillAll(juce::Colour::fromRGB(24, 25, 27));
            juce::Random random(42);
            for (int i = 0; i < size * size / 9; ++i)
            {
                const float x = random.nextFloat() * size;
                const float y = random.nextFloat() * size;
                const float r = 0.9f + random.nextFloat() * 1.5f;
                // Draw wrapped copies so the tile has no visible seams.
                for (float ox : { 0.0f, -static_cast<float>(size), static_cast<float>(size) })
                    for (float oy : { 0.0f, -static_cast<float>(size), static_cast<float>(size) })
                    {
                        const float gx = x + ox, gy = y + oy;
                        if (gx < -4.0f || gy < -4.0f || gx > size + 4.0f || gy > size + 4.0f) continue;
                        g.setColour(juce::Colours::black.withAlpha(0.35f));
                        g.fillEllipse(gx - r + 0.7f, gy - r + 0.8f, r * 2.0f, r * 2.0f);
                        g.setColour(juce::Colour::fromRGB(40, 42, 45));
                        g.fillEllipse(gx - r, gy - r, r * 2.0f, r * 2.0f);
                        g.setColour(juce::Colours::white.withAlpha(0.06f));
                        g.fillEllipse(gx - r * 0.6f, gy - r * 0.7f, r * 0.9f, r * 0.8f);
                    }
            }
            return image;
        }();
        return tile;
    }

    // Text cut into the metal: a dark line below, then the lettering.
    inline void drawEngraved(juce::Graphics& g, const juce::String& textToDraw,
                             juce::Rectangle<int> area, juce::Justification justification,
                             juce::Colour colour)
    {
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.drawText(textToDraw, area.translated(0, 1), justification, false);
        g.setColour(colour);
        g.drawText(textToDraw, area, justification, false);
    }

    // Gloss-black surface: dark gradient with a soft reflection on the top half.
    inline void fillGloss(juce::Graphics& g, const juce::Path& shape, juce::Rectangle<float> r,
                          float brightness = 1.0f)
    {
        g.setGradientFill(juce::ColourGradient(glossTop().brighter(brightness - 1.0f), r.getX(), r.getY(),
                                               glossBottom(), r.getX(), r.getBottom(), false));
        g.fillPath(shape);
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.09f), r.getX(), r.getY(),
                                               juce::Colours::white.withAlpha(0.0f), r.getX(),
                                               r.getCentreY(), false));
        g.fillPath(shape);
    }

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

    // Marks a rotary slider whose value arc starts at the centre (Pan).
    inline const juce::Identifier& bipolarKnobProperty()
    {
        static const juce::Identifier id { "osBipolarKnob" };
        return id;
    }

    // Marks a slider driven by turning the lens ring: the panel draws the
    // lens, so the slider itself paints nothing.
    inline const juce::Identifier& lensRingProperty()
    {
        static const juce::Identifier id { "osLensRing" };
        return id;
    }

    // Marks the button that triggers the camera: drawn with a red shutter dot.
    inline const juce::Identifier& shutterButtonProperty()
    {
        static const juce::Identifier id { "osShutterButton" };
        return id;
    }

    class LookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            if (auto typeface = bodyTypeface())
                setDefaultSansSerifTypeface(typeface);

            setColour(juce::ResizableWindow::backgroundColourId, background());
            setColour(juce::Slider::textBoxTextColourId, text());
            setColour(juce::Slider::textBoxBackgroundColourId, plotFill());
            setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::black.withAlpha(0.6f));
            setColour(juce::Slider::backgroundColourId, recess());
            setColour(juce::Slider::thumbColourId, text());
            setColour(juce::Label::textColourId, text());
            setColour(juce::TextButton::buttonColourId, glossBottom());
            setColour(juce::TextButton::buttonOnColourId, glossTop());
            setColour(juce::TextButton::textColourOffId, text());
            setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            setColour(juce::ComboBox::backgroundColourId, plotFill());
            setColour(juce::ComboBox::textColourId, text());
            setColour(juce::ComboBox::outlineColourId, juce::Colours::black.withAlpha(0.6f));
            setColour(juce::ComboBox::arrowColourId, muted());
            setColour(juce::PopupMenu::backgroundColourId, juce::Colour::fromRGB(24, 25, 28));
            setColour(juce::PopupMenu::textColourId, text());
            setColour(juce::PopupMenu::highlightedBackgroundColourId, panel());
            setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour(juce::TooltipWindow::backgroundColourId, juce::Colour::fromRGB(24, 25, 28));
            setColour(juce::TooltipWindow::textColourId, text());
            setColour(juce::TooltipWindow::outlineColourId, panelEdge());
        }

        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float position, float startAngle, float endAngle,
                              juce::Slider& slider) override
        {
            if (slider.getProperties()[lensRingProperty()])
                return;
            const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
            const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
            const float radius = juce::jmin(static_cast<float>(width), static_cast<float>(height)) * 0.41f;
            if (radius < 6.0f)
                return;

            const float angle = startAngle + position * (endAngle - startAngle);
            auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
            // Arc thickness follows the dial size so small and large knobs match.
            const float stroke = juce::jlimit(2.0f, 3.2f, radius * 0.13f);
            const auto rounded = juce::PathStrokeType(stroke, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded);

            // Scale ring cut into the plate.
            juce::Path track;
            track.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
            g.setColour(recess());
            g.strokePath(track, rounded);

            // Bipolar knobs (e.g. Pan) fill from the centre, not from the minimum.
            const bool bipolar = static_cast<bool>(slider.getProperties()[bipolarKnobProperty()]);
            const float arcFrom = bipolar ? 0.5f * (startAngle + endAngle) : startAngle;
            juce::Path value;
            value.addCentredArc(cx, cy, radius, radius, 0.0f,
                                juce::jmin(arcFrom, angle),
                                juce::jmax(arcFrom, angle) + 0.001f, true);
            g.setColour(accent);
            g.strokePath(value, rounded);

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

            // Gloss-black dial with a knurled grip, like a lens ring.
            const float grip = radius * 0.78f;
            const float cap = radius * 0.60f;
            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.fillEllipse(cx - grip, cy - grip + 1.5f, grip * 2.0f, grip * 2.0f);   // drop shadow
            g.setColour(juce::Colour::fromRGB(20, 21, 24));
            g.fillEllipse(cx - grip, cy - grip, grip * 2.0f, grip * 2.0f);

            constexpr int ridges = 36;
            g.setColour(juce::Colour::fromRGB(58, 61, 66));
            for (int i = 0; i < ridges; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * static_cast<float>(i) / ridges;
                g.drawLine(cx + std::sin(a) * (cap + 1.0f), cy - std::cos(a) * (cap + 1.0f),
                           cx + std::sin(a) * (grip - 0.8f), cy - std::cos(a) * (grip - 0.8f), 1.0f);
            }

            juce::ColourGradient body(juce::Colour::fromRGB(70, 73, 79), cx - cap * 0.45f, cy - cap * 0.65f,
                                      juce::Colour::fromRGB(9, 10, 12), cx + cap * 0.5f, cy + cap, true);
            g.setGradientFill(body);
            g.fillEllipse(cx - cap, cy - cap, cap * 2.0f, cap * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.10f));
            g.drawEllipse(cx - cap, cy - cap, cap * 2.0f, cap * 2.0f, 0.8f);

            // White index line, engraved on the cap.
            const float inner = cap * 0.25f;
            const float tip = cap * 0.88f;
            g.setColour(text());
            g.drawLine(cx + std::sin(angle) * inner, cy - std::cos(angle) * inner,
                       cx + std::sin(angle) * tip, cy - std::cos(angle) * tip,
                       juce::jlimit(1.6f, 2.4f, radius * 0.09f));
        }

        void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                  bool isOver, bool isDown) override
        {
            const auto r = button.getLocalBounds().toFloat().reduced(0.5f);
            const float corner = 4.0f;
            const bool left = button.isConnectedOnLeft(), right = button.isConnectedOnRight();
            juce::Path shape;
            shape.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), corner, corner,
                                      ! left, ! right, ! left, ! right);

            const bool on = button.getToggleState();
            fillGloss(g, shape, r, isDown ? 0.85f : (isOver || on ? 1.25f : 1.0f));
            g.setColour(juce::Colours::black.withAlpha(0.8f));
            g.strokePath(shape, juce::PathStrokeType(1.0f));

            // Selected segment: a thin red bar, like a mode-dial marker.
            if (on && ! button.getProperties()[shutterButtonProperty()])
            {
                g.setColour(red());
                g.fillRect(r.withSizeKeepingCentre(juce::jmin(28.0f, r.getWidth() * 0.4f), 2.0f)
                            .withY(r.getBottom() - 4.0f));
            }

            if (button.getProperties()[shutterButtonProperty()])
            {
                const float d = juce::jmin(9.0f, r.getHeight() * 0.32f);
                const auto dot = juce::Rectangle<float>(d, d).withCentre({ r.getX() + 15.0f, r.getCentreY() });
                g.setColour(red().withAlpha(on ? 0.35f : 0.18f));
                g.fillEllipse(dot.expanded(3.0f));
                g.setColour(on ? red().brighter(0.3f) : red());
                g.fillEllipse(dot);
            }
        }

        void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool over, bool down) override
        {
            if (! button.getProperties()[shutterButtonProperty()])
                return juce::LookAndFeel_V4::drawButtonText(g, button, over, down);
            g.setFont(getTextButtonFont(button, button.getHeight()));
            g.setColour(button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                                  : juce::TextButton::textColourOffId)
                            .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
            g.drawText(button.getButtonText(), button.getLocalBounds().withTrimmedLeft(22).withTrimmedRight(6),
                       juce::Justification::centred, false);
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
        juce::Font getComboBoxFont(juce::ComboBox&) override { return font(13.0f); }
        juce::Font getPopupMenuFont() override { return font(14.0f); }

        juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
        {
            return font(juce::jmin(13.5f, static_cast<float>(buttonHeight) * 0.48f))
                       .withExtraKerningFactor(0.05f);
        }
    };

    // Keyboard with ivory-white keys and gloss-black sharps; pressed keys
    // light up red.
    class MarbleKeyboard final : public juce::MidiKeyboardComponent
    {
    public:
        using juce::MidiKeyboardComponent::MidiKeyboardComponent;

        void drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area,
                           bool isDown, bool isOver, juce::Colour, juce::Colour) override
        {
            g.setGradientFill(juce::ColourGradient(marble().brighter(0.03f), 0.0f, area.getY(),
                                                   marble().darker(0.04f), 0.0f, area.getBottom(), false));
            g.fillRect(area);
            // Darker towards the front edge, like a rounded key lip.
            g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.0f), 0.0f, area.getBottom() - 26.0f,
                                                   juce::Colours::black.withAlpha(0.10f), 0.0f, area.getBottom(), false));
            g.fillRect(area);
            g.setColour(juce::Colours::black.withAlpha(0.30f));
            g.drawRect(area, 0.8f);
            if (isDown || isOver)
            {
                g.setColour(red().withAlpha(isDown ? 0.45f : 0.10f));
                g.fillRect(area.reduced(1.0f));
            }
            if (note % 12 == 0)
            {
                g.setColour(muted().darker(0.3f));
                g.setFont(font(10.5f));
                g.drawText(getWhiteNoteText(note), area.withTrimmedTop(area.getHeight() - 20.0f),
                           juce::Justification::centred, false);
            }
        }

        void drawBlackNote(int, juce::Graphics& g, juce::Rectangle<float> area,
                           bool isDown, bool isOver, juce::Colour) override
        {
            juce::Path key;
            key.addRoundedRectangle(area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                                    2.5f, 2.5f, false, false, true, true);
            fillGloss(g, key, area, isOver ? 1.4f : 1.15f);
            g.setColour(juce::Colours::black);
            g.strokePath(key, juce::PathStrokeType(1.0f));
            if (isDown)
            {
                g.setColour(red().withAlpha(0.6f));
                g.fillPath(key);
            }
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

        // Camera-style switch: gloss pill with a status LED, red when ON.
        void paintButton(juce::Graphics& g, bool isOver, bool isDown) override
        {
            const bool active = getToggleState();
            const auto r = getLocalBounds().toFloat().reduced(0.5f);
            juce::Path pill;
            pill.addRoundedRectangle(r, r.getHeight() * 0.5f);
            fillGloss(g, pill, r, isDown ? 0.85f : (isOver ? 1.2f : 1.0f));
            g.setColour(juce::Colours::black.withAlpha(0.8f));
            g.strokePath(pill, juce::PathStrokeType(1.0f));

            const float d = 6.0f;
            const auto led = juce::Rectangle<float>(d, d).withCentre({ r.getX() + 11.0f, r.getCentreY() });
            if (active)
            {
                g.setColour(red().withAlpha(0.30f));
                g.fillEllipse(led.expanded(3.0f));
            }
            g.setColour(active ? red().brighter(0.15f) : juce::Colour::fromRGB(70, 72, 76));
            g.fillEllipse(led);

            g.setColour(active ? osui::text() : osui::muted());
            g.setFont(font(10.5f).withExtraKerningFactor(0.08f));
            g.drawText(active ? "ON" : "OFF", getLocalBounds().withTrimmedLeft(16),
                       juce::Justification::centred, false);
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

    // Gloss-black screen set into the plate, with a faint grid and reflection.
    inline void drawPlot(juce::Graphics& g, juce::Rectangle<float> rect)
    {
        g.setColour(juce::Colours::white.withAlpha(0.07f));                 // bevel light
        g.drawRoundedRectangle(rect.translated(0.0f, 1.0f), 6.0f, 1.0f);
        g.setColour(plotFill());
        g.fillRoundedRectangle(rect, 6.0f);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(rect, 6.0f, 1.2f);

        auto inner = rect.reduced(8.0f);
        g.setColour(juce::Colour::fromRGB(40, 42, 46).withAlpha(0.7f));
        for (int i = 1; i < 4; ++i)
        {
            const auto x = inner.getX() + inner.getWidth() * i / 4.0f;
            const auto y = inner.getY() + inner.getHeight() * i / 4.0f;
            g.drawVerticalLine(static_cast<int>(x), inner.getY(), inner.getBottom());
            g.drawHorizontalLine(static_cast<int>(y), inner.getX(), inner.getRight());
        }

        // Lens-coating tints: violet from the top-left, green from the bottom-right.
        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path screen;
            screen.addRoundedRectangle(rect.reduced(1.0f), 5.0f);
            g.reduceClipRegion(screen);
            const float reach = juce::jmax(rect.getWidth(), rect.getHeight()) * 0.55f;
            g.setGradientFill(juce::ColourGradient(coatingViolet().withAlpha(0.16f),
                                                   rect.getX() + rect.getWidth() * 0.12f, rect.getY() + rect.getHeight() * 0.10f,
                                                   coatingViolet().withAlpha(0.0f),
                                                   rect.getX() + rect.getWidth() * 0.12f + reach,
                                                   rect.getY() + rect.getHeight() * 0.10f, true));
            g.fillRect(rect);
            g.setGradientFill(juce::ColourGradient(coatingGreen().withAlpha(0.10f),
                                                   rect.getRight() - rect.getWidth() * 0.12f, rect.getBottom() - rect.getHeight() * 0.10f,
                                                   coatingGreen().withAlpha(0.0f),
                                                   rect.getRight() - rect.getWidth() * 0.12f - reach * 0.8f,
                                                   rect.getBottom() - rect.getHeight() * 0.10f, true));
            g.fillRect(rect);
        }

        // Glass reflection on the upper part.
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.06f), rect.getX(), rect.getY(),
                                               juce::Colours::white.withAlpha(0.0f), rect.getX(),
                                               rect.getY() + rect.getHeight() * 0.45f, false));
        g.fillRoundedRectangle(rect.reduced(1.5f), 5.0f);
    }

    // Text set along a circle. Top text reads clockwise over the top, bottom
    // text reads left to right under the bottom; both stay upright.
    inline void drawTextOnArc(juce::Graphics& g, const juce::String& textToDraw,
                              juce::Point<float> centre, float radius, bool bottom,
                              const juce::Font& textFont, juce::Colour colour)
    {
        g.setFont(textFont);
        g.setColour(colour);
        float total = 0.0f;
        for (auto c : textToDraw)
            total += juce::GlyphArrangement::getStringWidth(textFont, juce::String::charToString(c));
        const float sweep = total / radius;
        const float centreAngle = bottom ? juce::MathConstants<float>::pi : 0.0f;
        const float h = textFont.getHeight();
        float run = 0.0f;
        for (auto c : textToDraw)
        {
            const auto ch = juce::String::charToString(c);
            const float w = juce::GlyphArrangement::getStringWidth(textFont, ch);
            const float offset = (run + w * 0.5f) / radius - sweep * 0.5f;
            run += w;
            const float angle = bottom ? centreAngle - offset : centreAngle + offset;
            juce::Graphics::ScopedSaveState state(g);
            g.addTransform(juce::AffineTransform::rotation(
                bottom ? angle - juce::MathConstants<float>::pi : angle, centre.x, centre.y));
            const float y = bottom ? centre.y + radius - h * 0.5f : centre.y - radius - h * 0.5f;
            g.drawText(ch, juce::Rectangle<float>(centre.x - w, y, w * 2.0f, h),
                       juce::Justification::centred, false);
        }
    }

    // A camera lens seen from the front: knurled gloss-black barrel, engraved
    // ring text, coated glass. The grip ring turns by ringAngle (radians), its
    // red mark showing the position. Returns the glass circle to draw into.
    inline juce::Rectangle<float> drawLens(juce::Graphics& g, juce::Rectangle<float> area,
                                           const juce::String& topText, const juce::String& bottomText,
                                           float ringAngle = 0.0f, bool ringActive = false)
    {
        const float r0 = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f - 1.0f;
        const auto c = area.getCentre();
        auto circle = [c](float r) { return juce::Rectangle<float>(r * 2.0f, r * 2.0f).withCentre(c); };

        // Shadow on the plate, then the barrel.
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillEllipse(circle(r0 + 1.0f).translated(0.0f, 3.0f));
        g.setGradientFill(juce::ColourGradient(juce::Colour::fromRGB(52, 55, 60), c.x, c.y - r0,
                                               juce::Colour::fromRGB(10, 11, 13), c.x, c.y + r0, false));
        g.fillEllipse(circle(r0));

        // Knurled grip band.
        const float gripInner = r0 * 0.88f;
        g.setColour(juce::Colour::fromRGB(16, 17, 19));
        g.drawEllipse(circle((r0 + gripInner) * 0.5f), r0 - gripInner);
        g.setColour(juce::Colour::fromRGB(62, 65, 70));
        constexpr int ridges = 120;
        for (int i = 0; i < ridges; ++i)
        {
            const float a = ringAngle + juce::MathConstants<float>::twoPi * static_cast<float>(i) / ridges;
            g.drawLine(c.x + std::sin(a) * (gripInner + 1.0f), c.y - std::cos(a) * (gripInner + 1.0f),
                       c.x + std::sin(a) * (r0 - 1.5f), c.y - std::cos(a) * (r0 - 1.5f), 1.0f);
        }
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawEllipse(circle(r0 - 0.5f), 1.0f);

        // Red mark on the grip: where the ring is turned to.
        {
            const float markRadius = (r0 + gripInner) * 0.5f;
            const float d = juce::jmax(5.0f, (r0 - gripInner) * 0.62f);
            const auto mark = juce::Rectangle<float>(d, d).withCentre(
                { c.x + std::sin(ringAngle) * markRadius, c.y - std::cos(ringAngle) * markRadius });
            g.setColour(juce::Colours::black);
            g.fillEllipse(mark.expanded(1.5f));
            if (ringActive)
            {
                g.setColour(red().withAlpha(0.35f));
                g.fillEllipse(mark.expanded(3.0f));
            }
            g.setColour(ringActive ? red().brighter(0.1f) : red().withMultipliedSaturation(0.35f).darker(0.4f));
            g.fillEllipse(mark);
        }

        // Satin name ring with engraved lettering.
        const float ringInner = r0 * 0.70f;
        g.setGradientFill(juce::ColourGradient(juce::Colour::fromRGB(30, 31, 35), c.x, c.y - gripInner,
                                               juce::Colour::fromRGB(12, 13, 15), c.x, c.y + gripInner, false));
        g.fillEllipse(circle(gripInner));
        g.setColour(juce::Colours::black);
        g.drawEllipse(circle(gripInner), 1.2f);
        const float textRadius = (gripInner + ringInner) * 0.5f;
        const auto ringFont = font(juce::jlimit(8.0f, 11.0f, r0 * 0.075f)).withExtraKerningFactor(0.12f);
        drawTextOnArc(g, topText, c, textRadius, false, ringFont, text().withAlpha(0.85f));
        drawTextOnArc(g, bottomText, c, textRadius, true, ringFont, muted());

        // Bright inner bezel, then the coated glass.
        g.setColour(juce::Colour::fromRGB(120, 124, 130));
        g.drawEllipse(circle(ringInner), 1.4f);
        const float glass = ringInner - 2.0f;
        g.setGradientFill(juce::ColourGradient(juce::Colour::fromRGB(4, 5, 7), c.x, c.y,
                                               juce::Colour::fromRGB(22, 24, 30), c.x + glass, c.y, true));
        g.fillEllipse(circle(glass));
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawEllipse(circle(glass * 0.72f), 1.0f);              // inner lens elements
        g.drawEllipse(circle(glass * 0.42f), 1.0f);

        // Anti-reflective coating tints.
        g.setGradientFill(juce::ColourGradient(coatingViolet().withAlpha(0.20f),
                                               c.x - glass * 0.35f, c.y - glass * 0.45f,
                                               coatingViolet().withAlpha(0.0f),
                                               c.x - glass * 0.35f + glass * 0.45f, c.y - glass * 0.45f, true));
        g.fillEllipse(circle(glass));
        g.setGradientFill(juce::ColourGradient(coatingGreen().withAlpha(0.12f),
                                               c.x + glass * 0.45f, c.y + glass * 0.40f,
                                               coatingGreen().withAlpha(0.0f),
                                               c.x + glass * 0.45f + glass * 0.35f, c.y + glass * 0.40f, true));
        g.fillEllipse(circle(glass));
        return circle(glass);
    }

    // Specular highlight over the glass, drawn after its contents.
    inline void drawLensGlare(juce::Graphics& g, juce::Rectangle<float> glass)
    {
        juce::Path glare;
        glare.addPieSegment(glass.reduced(glass.getWidth() * 0.06f), -1.45f, -0.25f, 0.82f);
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.14f),
                                               glass.getX(), glass.getY(),
                                               juce::Colours::white.withAlpha(0.0f),
                                               glass.getCentreX(), glass.getCentreY(), false));
        g.fillPath(glare);
    }

    class ModulePanel : public juce::Component
    {
    public:
        explicit ModulePanel(juce::String title, juce::String eyebrow = {})
            : heading(std::move(title)), detail(std::move(eyebrow)) {}

        // Machined top plate with an engraved title.
        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(0.8f);
            g.setGradientFill(juce::ColourGradient(panel().brighter(0.06f), 0.0f, bounds.getY(),
                                                   panel().darker(0.08f), 0.0f, bounds.getBottom(), false));
            g.fillRoundedRectangle(bounds, 7.0f);
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.drawRoundedRectangle(bounds, 7.0f, 1.2f);
            g.setColour(juce::Colours::white.withAlpha(0.10f));               // top edge catch-light
            g.drawHorizontalLine(static_cast<int>(bounds.getY()) + 1, bounds.getX() + 7.0f, bounds.getRight() - 7.0f);

            g.setFont(titleFont(19.0f));
            drawEngraved(g, heading.toUpperCase(), { 16, 11, getWidth() - 32, 22 },
                         juce::Justification::centredLeft, text());
            // The upper-right header is reserved for the power switch.
            if (detail.isNotEmpty() && powerIndicator == nullptr)
            {
                g.setFont(font(10.0f).withExtraKerningFactor(0.08f));
                drawEngraved(g, detail.toUpperCase(), { 16, 12, getWidth() - 32, 18 },
                             juce::Justification::centredRight, muted());
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

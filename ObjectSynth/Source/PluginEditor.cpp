#include "PluginEditor.h"

ObjectSynthAudioProcessorEditor::ObjectSynthAudioProcessorEditor(
    ObjectSynthAudioProcessor& processor)
    : juce::AudioProcessorEditor(&processor),
      synthProcessor(processor),
      presetBar(processor),
      oscillatorPanel(processor),
      unisonPanel(processor.getAPVTS()),
      envelopePanel(processor.getAPVTS()),
      filterPanel(processor.getAPVTS(), processor),
      distortionPanel(processor.getAPVTS()),
      chorusPanel(processor.getAPVTS()),
      filterEnvelopePanel(processor.getAPVTS()),
      lfoPanel(processor.getAPVTS(), processor),
      outputPanel(processor),
      midiKeyboard(processor.getKeyboardState(),
                   juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&theme);
    addAndMakeVisible(uiCanvas);

    uiCanvas.addAndMakeVisible(presetBar);
    uiCanvas.addAndMakeVisible(oscillatorPanel);
    uiCanvas.addAndMakeVisible(unisonPanel);
    uiCanvas.addAndMakeVisible(envelopePanel);
    uiCanvas.addAndMakeVisible(filterPanel);
    uiCanvas.addAndMakeVisible(distortionPanel);
    uiCanvas.addAndMakeVisible(chorusPanel);
    uiCanvas.addAndMakeVisible(filterEnvelopePanel);
    uiCanvas.addAndMakeVisible(lfoPanel);
    uiCanvas.addAndMakeVisible(outputPanel);
    uiCanvas.addAndMakeVisible(midiKeyboard);

    // Five octaves (C1..C6): on the wider canvas the keys keep about the
    // same proportions as the old four-octave keyboard.
    midiKeyboard.setAvailableRange(36, 96);
    midiKeyboard.setLowestVisibleKey(36);
    midiKeyboard.setScrollButtonsVisible(false);
    midiKeyboard.setMouseClickGrabsKeyboardFocus(true);

    constexpr double aspect = static_cast<double>(designWidth) / designHeight; // 16:10
    setResizable(true, true);
    setResizeLimits(1120, 700, 2448, 1530);
    getConstrainer()->setFixedAspectRatio(aspect);

    // Open as large as fits comfortably on the main screen, never above
    // the 1:1 design size (1632 x 1020).
    double scale = 0.9;
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userArea;
        scale = juce::jmin(1.0, area.getWidth() * 0.92 / designWidth,
                           area.getHeight() * 0.85 / designHeight);
    }
    const int width = juce::jlimit(1120, designWidth, juce::roundToInt(designWidth * scale));
    setSize(width, juce::roundToInt(width / aspect));

    // Live LFO modulation display on the knobs.
    collectModulatableKnobs(uiCanvas);
    startTimerHz(30);
}

ObjectSynthAudioProcessorEditor::~ObjectSynthAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void ObjectSynthAudioProcessorEditor::collectModulatableKnobs(juce::Component& parent)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* slider = dynamic_cast<juce::Slider*>(child))
        {
            const auto id = slider->getProperties()[osui::parameterIdProperty()].toString();
            const auto destination = ObjectSynthAudioProcessor::destinationForParameter(id);
            if (destination != ModDestination::None)
                modulatedKnobs.push_back({slider, destination});
        }
        collectModulatableKnobs(*child);
    }
}

void ObjectSynthAudioProcessorEditor::timerCallback()
{
    for (const auto& knob : modulatedKnobs)
    {
        auto& properties = knob.slider->getProperties();
        const auto& previous = properties[osui::modulatedPositionProperty()];
        const float value = synthProcessor.getModulatedValueForUI(knob.destination);

        bool show = false;
        double position = 0.0;
        if (std::isfinite(value))
        {
            position = knob.slider->valueToProportionOfLength(static_cast<double>(value));
            const double setPosition = knob.slider->valueToProportionOfLength(knob.slider->getValue());
            show = std::isfinite(position) && std::abs(position - setPosition) > 0.002;
        }

        if (show)
        {
            if (previous.isVoid() || std::abs(static_cast<double>(previous) - position) > 0.001)
            {
                properties.set(osui::modulatedPositionProperty(), position);
                knob.slider->repaint();
            }
        }
        else if (! previous.isVoid())
        {
            properties.remove(osui::modulatedPositionProperty());
            knob.slider->repaint();
        }
    }
}

// Camera body: pebbled leatherette behind the metal plates, lit from the top.
void ObjectSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setTiledImageFill(osui::leatheretteTile(), 0, 0, 1.0f);
    g.fillRect(r);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.04f), 0.0f, 0.0f,
                                           juce::Colours::black.withAlpha(0.25f), 0.0f, r.getBottom(), false));
    g.fillRect(r);
}

void ObjectSynthAudioProcessorEditor::resized()
{
    const float scaleX = static_cast<float>(getWidth()) / designWidth;
    const float scaleY = static_cast<float>(getHeight()) / designHeight;
    const float scale = juce::jmin(scaleX, scaleY);
    const float offsetX = (getWidth() - designWidth * scale) * 0.5f;
    const float offsetY = (getHeight() - designHeight * scale) * 0.5f;

    uiCanvas.setBounds(0, 0, designWidth, designHeight);
    uiCanvas.setTransform(juce::AffineTransform::scale(scale)
                              .translated(offsetX, offsetY));

    // Layout V6 (logical 1632 x 1020 canvas, 16:10): the sketch's grid on the left,
    // and every effect of the output chain in one column on the right, in
    // signal order (distortion -> chorus -> volume/pan).
    //
    //   +-----------+-------------+-----------+  +-----------+
    //   | OSCILLATOR| ENVELOPE    | FILTER    |  | DIST.     |
    //   |           +-------------+ BANK      |  +-----------+
    //   |           | UNISON      |           |  | CHORUS    |
    //   +-----------+--+----------+-----------+  +-----------+
    //   | FILTER ENVELOPE | LFO / MOD MATRIX  |  | OUTPUT    |
    //   +-----------------+-------------------+  +-----------+
    //   | MIDI KEYBOARD                                      |
    //   +----------------------------------------------------+
    //
    // The effects column is split at the same height as the two rows, so
    // all horizontal panel edges line up.
    constexpr int margin = 12;
    constexpr int gap = 8;
    constexpr int presetHeight = 36;
    constexpr int topRowHeight = 480;
    constexpr int keyboardHeight = 160;

    // Left grid = the V5 sketch widths (384 / 508 / 420 on top), so nothing
    // is squeezed by the effects column.
    constexpr int effectsWidth = 272;
    constexpr int distortionHeight = 200;

    constexpr int oscillatorWidth = 384;
    constexpr int centreWidth = 508;
    constexpr int envelopeHeight = 262;
    constexpr int filterEnvelopeWidth = 600;

    auto area = uiCanvas.getLocalBounds().reduced(margin);

    presetBar.setBounds(area.removeFromTop(presetHeight));
    area.removeFromTop(gap);

    auto keyboardArea = area.removeFromBottom(keyboardHeight);
    area.removeFromBottom(gap);

    // --- Effects column (right): DIST + CHORUS beside the top row,
    //     OUTPUT beside the bottom row.
    auto effectsColumn = area.removeFromRight(effectsWidth);
    area.removeFromRight(gap);
    auto effectsTop = effectsColumn.removeFromTop(topRowHeight);
    effectsColumn.removeFromTop(gap);
    distortionPanel.setBounds(effectsTop.removeFromTop(distortionHeight));
    effectsTop.removeFromTop(gap);
    chorusPanel.setBounds(effectsTop);
    outputPanel.setBounds(effectsColumn);

    auto topRow = area.removeFromTop(topRowHeight);
    area.removeFromTop(gap);
    auto bottomRow = area;

    // --- Top row: OSCILLATOR | ENVELOPE + UNISON | FILTER BANK
    oscillatorPanel.setBounds(topRow.removeFromLeft(oscillatorWidth));
    topRow.removeFromLeft(gap);

    auto centreColumn = topRow.removeFromLeft(centreWidth);
    topRow.removeFromLeft(gap);
    envelopePanel.setBounds(centreColumn.removeFromTop(envelopeHeight));
    centreColumn.removeFromTop(gap);
    unisonPanel.setBounds(centreColumn);

    filterPanel.setBounds(topRow);

    // --- Bottom row: FILTER ENVELOPE | LFO
    filterEnvelopePanel.setBounds(bottomRow.removeFromLeft(filterEnvelopeWidth));
    bottomRow.removeFromLeft(gap);
    lfoPanel.setBounds(bottomRow);

    // --- Keyboard: full width, five octaves + top C (36 white keys).
    // Leatherette cheeks around the keys, like the covering of a camera body.
    const auto keys = keyboardArea.reduced(22, 0).withTrimmedTop(10);
    midiKeyboard.setBounds(keys);
    midiKeyboard.setKeyWidth(static_cast<float>(keys.getWidth()) / 36.0f);
}

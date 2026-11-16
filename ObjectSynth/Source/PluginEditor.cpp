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
    uiCanvas.addAndMakeVisible(midiKeyboard);

    midiKeyboard.setAvailableRange(48, 95);
    midiKeyboard.setLowestVisibleKey(48);
    midiKeyboard.setScrollButtonsVisible(false);
    midiKeyboard.setMouseClickGrabsKeyboardFocus(true);

    setResizable(true, true);
    setResizeLimits(952, 714, 2040, 1530);
    getConstrainer()->setFixedAspectRatio(4.0 / 3.0);
    setSize(1224, 918);

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

void ObjectSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(osui::background());
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

    // Layout V5, following the hand-drawn sketch (logical 1360 x 1020 canvas):
    //
    //   +--------------+-------------------+-------------+
    //   | OSCILLATOR   | ENVELOPE          | FILTER BANK |   top row    ~48%
    //   |              +-------------------+             |
    //   |              | UNISON            |             |
    //   +----------+---+-----------+-------+-------------+
    //   | DIST.    | FILTER ENV    | LFO / MOD MATRIX    |   bottom row ~30%
    //   | CHORUS   |               |                     |
    //   +----------+---------------+---------------------+
    //   | MIDI KEYBOARD                                  |   ~16%
    //   +------------------------------------------------+
    //
    // The two rows use different column splits on purpose, as in the sketch:
    // top 29 / 38.5 / 32.5 %, bottom 20 / 37 / 43 %.
    constexpr int margin = 12;
    constexpr int gap = 8;
    constexpr int presetHeight = 36;
    constexpr int topRowHeight = 480;
    constexpr int keyboardHeight = 160;

    constexpr int oscillatorWidth = 384;
    constexpr int centreWidth = 508;
    constexpr int envelopeHeight = 262;

    constexpr int effectsWidth = 268;
    constexpr int filterEnvelopeWidth = 488;
    constexpr int distortionHeight = 104;

    auto area = uiCanvas.getLocalBounds().reduced(margin);

    presetBar.setBounds(area.removeFromTop(presetHeight));
    area.removeFromTop(gap);

    auto keyboardArea = area.removeFromBottom(keyboardHeight);
    area.removeFromBottom(gap);

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

    // --- Bottom row: DIST + CHORUS | FILTER ENVELOPE | LFO
    auto effectsColumn = bottomRow.removeFromLeft(effectsWidth);
    bottomRow.removeFromLeft(gap);
    distortionPanel.setBounds(effectsColumn.removeFromTop(distortionHeight));
    effectsColumn.removeFromTop(gap);
    chorusPanel.setBounds(effectsColumn);

    filterEnvelopePanel.setBounds(bottomRow.removeFromLeft(filterEnvelopeWidth));
    bottomRow.removeFromLeft(gap);
    lfoPanel.setBounds(bottomRow);

    // --- Keyboard: full width, four octaves (28 white keys).
    midiKeyboard.setBounds(keyboardArea);
    midiKeyboard.setKeyWidth(static_cast<float>(keyboardArea.getWidth()) / 28.0f);
}

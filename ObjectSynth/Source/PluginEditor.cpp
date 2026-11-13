#include "PluginEditor.h"

ObjectSynthAudioProcessorEditor::ObjectSynthAudioProcessorEditor(
    ObjectSynthAudioProcessor& processor)
    : juce::AudioProcessorEditor(&processor),
      oscillatorPanel(processor),
      envelopePanel(processor.getAPVTS()),
      filterPanel(processor.getAPVTS(), processor),
      distortionPanel(processor.getAPVTS()),
      chorusPanel(processor.getAPVTS()),
      midiKeyboard(processor.getKeyboardState(),
                   juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&theme);

    addAndMakeVisible(oscillatorPanel);
    addAndMakeVisible(envelopePanel);
    addAndMakeVisible(filterPanel);
    addAndMakeVisible(distortionPanel);
    addAndMakeVisible(chorusPanel);

    addAndMakeVisible(midiKeyboard);
    midiKeyboard.setAvailableRange(48, 95);
    midiKeyboard.setLowestVisibleKey(48);
    midiKeyboard.setScrollButtonsVisible(false);
    midiKeyboard.setMouseClickGrabsKeyboardFocus(true);

    // A fixed canvas keeps the graphs legible and avoids distorted layouts.
    setResizable(false, false);
    setSize(1160, 1000);
}

ObjectSynthAudioProcessorEditor::~ObjectSynthAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void ObjectSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(osui::background());
}

void ObjectSynthAudioProcessorEditor::resized()
{
    constexpr int gap = 10;
    auto area = getLocalBounds().reduced(14);

    // The keyboard is always visible and no Play/Stop gate is required.
    const auto keyboardArea = area.removeFromBottom(150);
    area.removeFromBottom(gap);

    // Effects are intentionally compact to leave more room for the filter plot.
    auto effectsArea = area.removeFromBottom(170);
    area.removeFromBottom(gap);

    // Dedicated, full-width frequency-response view and filter controls.
    const auto filterArea = area.removeFromBottom(350);
    area.removeFromBottom(gap);

    // Waveform and envelope previews occupy the upper part of the fixed canvas.
    auto synthArea = area;
    oscillatorPanel.setBounds(synthArea.removeFromLeft(350));
    synthArea.removeFromLeft(gap);
    envelopePanel.setBounds(synthArea);

    filterPanel.setBounds(filterArea);

    distortionPanel.setBounds(effectsArea.removeFromLeft(365));
    effectsArea.removeFromLeft(gap);
    chorusPanel.setBounds(effectsArea);


    auto keyboardBounds = keyboardArea.reduced(4, 4);

    midiKeyboard.setBounds(keyboardBounds);

    // Four octaves = 28 white keys.
    constexpr float numWhiteKeys = 28.0f;

    const float keyWidth =
        static_cast<float>(keyboardBounds.getWidth()) / numWhiteKeys;

    midiKeyboard.setKeyWidth(keyWidth);

}

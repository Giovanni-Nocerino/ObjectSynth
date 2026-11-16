#include "PluginEditor.h"

ObjectSynthAudioProcessorEditor::ObjectSynthAudioProcessorEditor(
    ObjectSynthAudioProcessor& processor)
    : juce::AudioProcessorEditor(&processor),
      oscillatorPanel(processor),
      envelopePanel(processor.getAPVTS()),
      filterPanel(processor.getAPVTS(), processor),
      distortionPanel(processor.getAPVTS()),
      chorusPanel(processor.getAPVTS()),
      filterEnvelopePanel(processor.getAPVTS()),
      lfoPanel(processor.getAPVTS(), processor),
      midiKeyboard(processor.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&theme);

    addAndMakeVisible(uiCanvas);

    uiCanvas.addAndMakeVisible(oscillatorPanel);
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

    // A fixed canvas keeps the graphs legible and avoids distorted layouts.
    setResizable(true, true);
    setResizeLimits(816, 612, 1920, 1440);
    getConstrainer()->setFixedAspectRatio(4.0 / 3.0);
    setSize(1156, 867);
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
    // 1. Calculate a uniform scale factor.
    const float scaleX = static_cast<float>(getWidth()) / designWidth;
    const float scaleY = static_cast<float>(getHeight()) / designHeight;
    const float scale = juce::jmin(scaleX, scaleY);

    // 2. Centre and scale the logical canvas.
    const float offsetX =(getWidth() - designWidth * scale) * 0.5f;
    const float offsetY =(getHeight() - designHeight * scale) * 0.5f;

    uiCanvas.setBounds(0, 0, designWidth, designHeight);
    uiCanvas.setTransform(juce::AffineTransform::scale(scale).translated(offsetX, offsetY));

    // 3. Layout in logical coordinates.
    constexpr int gap = 10;
    auto area = uiCanvas.getLocalBounds().reduced(14);

    // MIDI keyboard.
    auto keyboardArea = area.removeFromBottom(142);
    area.removeFromBottom(gap);

    // Filter Envelope and future LFO.
    auto modulationArea = area.removeFromBottom(255);
    area.removeFromBottom(gap);

    // Effects column and main synth modules.
    auto effectsArea = area.removeFromLeft(195);
    area.removeFromLeft(gap);

    // Oscillator and amplitude envelope.
    auto topArea = area.removeFromTop(265);
    area.removeFromTop(gap);

    oscillatorPanel.setBounds(
        topArea.removeFromLeft(405)
    );

    topArea.removeFromLeft(gap);
    envelopePanel.setBounds(topArea);

    // Main FilterBank.
    filterPanel.setBounds(area);

    // Effects stacked vertically.
    distortionPanel.setBounds(
        effectsArea.removeFromTop(180)
    );

    effectsArea.removeFromTop(gap);
    chorusPanel.setBounds(effectsArea);

    // Filter Envelope and LFO.
    filterEnvelopePanel.setBounds(
        modulationArea.removeFromLeft(710)
    );

    modulationArea.removeFromLeft(gap);
    lfoPanel.setBounds(modulationArea);

    // MIDI keyboard.
    auto keyboardBounds = keyboardArea.reduced(4);

    midiKeyboard.setBounds(keyboardBounds);

    constexpr float numWhiteKeys = 28.0f;

    midiKeyboard.setKeyWidth(
        static_cast<float>(keyboardBounds.getWidth())
        / numWhiteKeys
    );
}
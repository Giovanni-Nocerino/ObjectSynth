#pragma once

#include "PluginProcessor.h"
#include "GUI/Theme.h"
#include "GUI/OscillatorPanel.h"
#include "GUI/EnvelopePanel.h"
#include "GUI/FilterBankPanel.h"
#include "GUI/EffectsPanel.h"
#include "GUI/FilterEnvelopePanel.h"
#include "GUI/LFOPanel.h"

// Layout and MIDI input only. Each independent panel owns its sliders/attachments.
class ObjectSynthAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ObjectSynthAudioProcessorEditor(ObjectSynthAudioProcessor& processor);
    ~ObjectSynthAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Must outlive the child controls that inherit the look-and-feel.
    osui::LookAndFeel theme;
    juce::Component uiCanvas;
    osui::OscillatorPanel oscillatorPanel;
    osui::EnvelopePanel envelopePanel;
    osui::FilterBankPanel filterPanel;
    osui::DistortionPanel distortionPanel;
    osui::ChorusPanel chorusPanel;
    osui::FilterEnvelopePanel filterEnvelopePanel;
    osui::LFOPanel lfoPanel;


    juce::MidiKeyboardComponent midiKeyboard;

    static constexpr int designWidth = 1360;
    static constexpr int designHeight = 1020;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ObjectSynthAudioProcessorEditor)
};

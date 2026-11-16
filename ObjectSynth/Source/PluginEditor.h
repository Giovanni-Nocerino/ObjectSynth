#pragma once

#include "PluginProcessor.h"
#include "GUI/Theme.h"
#include "GUI/PresetBar.h"
#include "GUI/OscillatorPanel.h"
#include "GUI/UnisonPanel.h"
#include "GUI/EnvelopePanel.h"
#include "GUI/FilterBankPanel.h"
#include "GUI/EffectsPanel.h"
#include "GUI/FilterEnvelopePanel.h"
#include "GUI/LFOPanel.h"

// V5: layout from the hand-drawn sketch (see resized() for the grid).
class ObjectSynthAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit ObjectSynthAudioProcessorEditor(ObjectSynthAudioProcessor& processor);
    ~ObjectSynthAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Pushes live modulated values onto the knobs (UI thread only).
    void timerCallback() override;
    void collectModulatableKnobs(juce::Component& parent);

    struct ModulatedKnob
    {
        juce::Slider* slider = nullptr;
        ModDestination destination = ModDestination::None;
    };

    ObjectSynthAudioProcessor& synthProcessor;
    std::vector<ModulatedKnob> modulatedKnobs;

    osui::LookAndFeel theme;
    juce::Component uiCanvas;
    osui::PresetBar presetBar;
    osui::OscillatorPanel oscillatorPanel;
    osui::UnisonPanel unisonPanel;
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

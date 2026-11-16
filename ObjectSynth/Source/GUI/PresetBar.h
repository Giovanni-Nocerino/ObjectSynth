#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"
#include <memory>

namespace osui
{
    // Simple portable preset files wrapping the existing APVTS + embedded wavetable state.
    class PresetBar final : public juce::Component
    {
    public:
        explicit PresetBar(ObjectSynthAudioProcessor& p) : processor(p)
        {
            saveButton.setButtonText("SAVE PRESET");
            loadButton.setButtonText("OPEN PRESET");
            saveButton.onClick = [this] { chooseSave(); };
            loadButton.onClick = [this] { chooseOpen(); };
            addAndMakeVisible(saveButton);
            addAndMakeVisible(loadButton);

            statusLabel.setText("8-voice polyphonic wavetable synthesizer",
                                juce::dontSendNotification);
            statusLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
            statusLabel.setColour(juce::Label::textColourId, muted());
            statusLabel.setJustificationType(juce::Justification::centredRight);
            addAndMakeVisible(statusLabel);
        }

        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour(panel());
            g.fillRoundedRectangle(r, 10.0f);
            g.setColour(panelEdge());
            g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, 1.0f);
            g.setColour(cyan());
            g.fillRoundedRectangle(12.0f, 10.0f, 3.0f, static_cast<float>(getHeight() - 20), 1.5f);
            g.setColour(text());
            g.setFont(juce::Font(juce::FontOptions(19.0f)));
            g.drawText("OBJECTSYNTH", 24, 6, 240, getHeight() - 12,
                       juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto right = getLocalBounds().reduced(10, 5);
            auto load = right.removeFromRight(126);
            right.removeFromRight(7);
            auto save = right.removeFromRight(126);
            right.removeFromRight(12);
            loadButton.setBounds(load);
            saveButton.setBounds(save);
            statusLabel.setBounds(right.removeFromRight(425));
        }

    private:
        void chooseSave()
        {
            chooser = std::make_unique<juce::FileChooser>(
                "Save ObjectSynth preset", juce::File(), "*.ospreset");
            const auto safe = juce::Component::SafePointer<PresetBar>(this);
            chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                                 juce::FileBrowserComponent::canSelectFiles |
                                 juce::FileBrowserComponent::warnAboutOverwriting,
                [safe](const juce::FileChooser& selected)
                {
                    if (safe == nullptr) return;
                    auto file = selected.getResult();
                    if (file == juce::File()) return;
                    file = file.withFileExtension(".ospreset");
                    juce::String status;
                    const bool ok = safe->processor.savePresetFile(file, status);
                    safe->showStatus(ok ? juce::String("Saved: ") + file.getFileName() : status, ok);
                });
        }

        void chooseOpen()
        {
            chooser = std::make_unique<juce::FileChooser>(
                "Open ObjectSynth preset", juce::File(), "*.ospreset");
            const auto safe = juce::Component::SafePointer<PresetBar>(this);
            chooser->launchAsync(juce::FileBrowserComponent::openMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                [safe](const juce::FileChooser& selected)
                {
                    if (safe == nullptr) return;
                    const auto file = selected.getResult();
                    if (file == juce::File()) return;
                    juce::String status;
                    const bool ok = safe->processor.loadPresetFile(file, status);
                    safe->showStatus(ok ? juce::String("Loaded: ") + file.getFileName() : status, ok);
                });
        }

        void showStatus(const juce::String& value, bool ok)
        {
            statusLabel.setText(value, juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, ok ? cyan() : orange());
            statusLabel.setTooltip(value);
        }

        ObjectSynthAudioProcessor& processor;
        juce::TextButton saveButton, loadButton;
        juce::Label statusLabel;
        std::unique_ptr<juce::FileChooser> chooser;
    };
}

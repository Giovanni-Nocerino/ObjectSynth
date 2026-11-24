#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"
#include <memory>

namespace osui
{
    // Enlarged print of the photographed object, shown while hovering the
    // thumbnail in the preset bar.
    class PhotoPrint final : public juce::Component
    {
    public:
        PhotoPrint() { setInterceptsMouseClicks(false, false); }

        void show(const juce::Image& photo, const juce::String& name)
        {
            image = photo;
            caption = name;
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced(6.0f);
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.fillRoundedRectangle(r.translated(0.0f, 4.0f), 4.0f);
            fillMarble(g, r, 9, 4.0f);
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.drawRoundedRectangle(r, 4.0f, 1.0f);

            auto picture = r.reduced(12.0f).withTrimmedBottom(26.0f);
            g.setColour(juce::Colours::black);
            g.fillRect(picture);
            if (image.isValid())
            {
                g.setOpacity(1.0f); // drawImage uses the current colour's alpha
                g.drawImage(image, picture, juce::RectanglePlacement::centred);
            }
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.drawRect(picture, 1.0f);

            g.setColour(ink());
            g.setFont(titleFont(18.0f, 0.12f));
            g.drawText(caption.toUpperCase(), r.removeFromBottom(32.0f).reduced(12.0f, 4.0f),
                       juce::Justification::centredLeft, true);
            const float d = 7.0f;
            g.setColour(red());
            g.fillEllipse(r.getRight() - 12.0f - d, r.getBottom() + 16.0f - d * 0.5f - 4.0f, d, d);
        }

    private:
        juce::Image image;
        juce::String caption;
    };

    // Simple portable preset files wrapping the existing APVTS + embedded wavetable state.
    // Also shows the photo of the object a camera preset was made from.
    class PresetBar final : public juce::Component, private juce::Timer
    {
    public:
        explicit PresetBar(ObjectSynthAudioProcessor& p) : processor(p)
        {
            startTimerHz(4);
            timerCallback(); // show the current preset's photo right away
            saveButton.setButtonText("SAVE PRESET");
            loadButton.setButtonText("OPEN PRESET");
            saveButton.onClick = [this] { chooseSave(); };
            loadButton.onClick = [this] { chooseOpen(); };
            addAndMakeVisible(saveButton);
            addAndMakeVisible(loadButton);

            // Empty until a preset is saved or opened (then it shows the result).
            statusLabel.setFont(font(11.5f));
            statusLabel.setColour(juce::Label::textColourId, ink().withAlpha(0.6f));
            statusLabel.setJustificationType(juce::Justification::centredRight);
            addAndMakeVisible(statusLabel);
        }

        // Marble top plate with the wordmark, like a camera's name plate.
        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced(0.5f);
            fillMarble(g, r, 3, 7.0f);
            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.drawRoundedRectangle(r, 7.0f, 1.0f);

            const float d = 10.0f;
            const auto dot = juce::Rectangle<float>(d, d).withCentre({ 24.0f, r.getCentreY() });
            g.setColour(red().withAlpha(0.25f));
            g.fillEllipse(dot.expanded(2.5f));
            g.setColour(red());
            g.fillEllipse(dot);

            g.setColour(ink());
            g.setFont(titleFont(28.0f, 0.16f));
            g.drawText("OBJECTSYNTH", 38, 4, 300, getHeight() - 6, juce::Justification::centredLeft);

            if (photo.isValid())
            {
                // Film frame: black border around the picture.
                const auto frame = thumbArea.toFloat();
                g.setColour(juce::Colours::black);
                g.fillRoundedRectangle(frame, 2.0f);
                g.setOpacity(1.0f);
                g.drawImage(photo, frame.reduced(2.0f), juce::RectanglePlacement::centred
                                                       | juce::RectanglePlacement::fillDestination);
                g.setColour(ink());
                g.setFont(titleFont(19.0f, 0.12f));
                g.drawText(photoName.toUpperCase(), thumbArea.getRight() + 10, 4, 260,
                           getHeight() - 6, juce::Justification::centredLeft, true);
            }
        }

        void resized() override
        {
            const int thumbHeight = getHeight() - 8;
            thumbArea = { 262, 4, thumbHeight * 4 / 3, thumbHeight };
            auto right = getLocalBounds().reduced(10, 5);
            auto load = right.removeFromRight(126);
            right.removeFromRight(7);
            auto save = right.removeFromRight(126);
            right.removeFromRight(12);
            loadButton.setBounds(load);
            saveButton.setBounds(save);
            statusLabel.setBounds(right.removeFromRight(425));
        }

        void mouseMove(const juce::MouseEvent& e) override { showPrint(thumbArea.contains(e.getPosition())); }
        void mouseExit(const juce::MouseEvent&) override { showPrint(false); }

    private:
        void timerCallback() override
        {
            const int version = processor.getObjectPhotoVersion();
            if (version == seenVersion) return;
            seenVersion = version;
            juce::String name;
            const auto jpeg = processor.getObjectPhoto(name);
            photo = jpeg.isEmpty() ? juce::Image()
                                   : juce::ImageFileFormat::loadFrom(jpeg.getData(), jpeg.getSize());
            photoName = name;
            print.show(photo, photoName);
            if (! photo.isValid()) showPrint(false);
            repaint();
        }

        void showPrint(bool visible)
        {
            visible = visible && photo.isValid();
            auto* parent = getParentComponent();
            if (parent == nullptr) return;
            if (print.getParentComponent() != parent)
                parent->addChildComponent(print);
            if (visible)
            {
                const int width = 340;
                const int height = width * 3 / 4 + 38;
                print.setBounds(getX() + thumbArea.getX() - 6, getBottom() + 4, width, height);
                print.toFront(false);
            }
            print.setVisible(visible);
        }

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
            statusLabel.setColour(juce::Label::textColourId, ok ? ink() : red().darker(0.2f));
            statusLabel.setTooltip(value);
        }

        ObjectSynthAudioProcessor& processor;
        juce::Image photo;
        juce::String photoName;
        int seenVersion = -1;
        juce::Rectangle<int> thumbArea;
        PhotoPrint print;
        juce::TextButton saveButton, loadButton;
        juce::Label statusLabel;
        std::unique_ptr<juce::FileChooser> chooser;
    };
}

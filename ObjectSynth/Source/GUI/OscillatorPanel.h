#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"
#include <array>
#include <cmath>
#include <memory>

namespace osui
{
    class OscillatorPanel final : public ModulePanel, private juce::Timer
    {
    public:
        explicit OscillatorPanel(ObjectSynthAudioProcessor& p)
            : ModulePanel("OSCILLATOR", "WAVETABLE"), processor(p)
        {
            setupPowerButton(*this, power, powerAttachment,
                             processor.getAPVTS(), "oscillatorEnabled");
            setPowerIndicator(power);

            cameraButton.setButtonText("CAMERA / CV");
            cameraButton.setEnabled(false); // Reserved for future image sonification.
            cameraButton.setTooltip("Future camera / Computer Vision source (not yet implemented)");
            addAndMakeVisible(cameraButton);

            const std::array<juce::String, 5> names {
                "SINE", "SAW", "SQUARE", "TRI", "CUSTOM"
            };
            for (std::size_t i = 0; i < buttons.size(); ++i)
            {
                auto& button = buttons[i];
                button.setButtonText(names[i]);
                button.setMouseClickGrabsKeyboardFocus(false);
                button.setWantsKeyboardFocus(false);
                // One continuous segmented selector, as in the sketch.
                int edges = 0;
                if (i > 0) edges |= juce::Button::ConnectedOnLeft;
                if (i + 1 < buttons.size()) edges |= juce::Button::ConnectedOnRight;
                button.setConnectedEdges(edges);
                button.onClick = [this, i]
                {
                    if (i == 4 && !processor.hasCustomWavetable()) return;
                    processor.setWaveform(static_cast<Waveform>(i));
                    refreshSelection();
                };
                addAndMakeVisible(button);
            }

            frameLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
            frameLabel.setColour(juce::Label::textColourId, muted());
            frameLabel.setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(frameLabel);

            frameSlider.setSliderStyle(juce::Slider::LinearHorizontal);
            frameSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            frameSlider.setRange(0.0, 1.0, 1.0);
            frameSlider.setMouseClickGrabsKeyboardFocus(false);
            frameSlider.setTooltip("Select the wavetable frame. Sounding notes crossfade to it.");
            frameSlider.onValueChange = [this]
            {
                if (processor.selectCustomFrame(
                        juce::roundToInt(frameSlider.getValue())))
                    refreshSelection();
            };
            addAndMakeVisible(frameSlider);

            loadButton.setButtonText("LOAD WAV");
            loadButton.setTooltip("Load single-cycle or 2048-sample multiframe WAV. Sounding notes crossfade to it.");
            loadButton.onClick = [this] { chooseWav(); };
            addAndMakeVisible(loadButton);

            fileLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
            fileLabel.setColour(juce::Label::textColourId, muted());
            fileLabel.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(fileLabel);

            statusLabel.setFont(juce::Font(juce::FontOptions(10.5f)));
            statusLabel.setColour(juce::Label::textColourId, muted());
            statusLabel.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(statusLabel);

            refreshSelection();
            startTimerHz(12);
        }

        void paint(juce::Graphics& g) override
        {
            ModulePanel::paint(g);
            if (plotArea.isEmpty()) return;

            drawPlot(g, plotArea.toFloat());
            auto plot = plotArea.toFloat().reduced(12.0f, 19.0f);
            if (plot.getWidth() <= 1.0f || plot.getHeight() <= 1.0f) return;

            g.setColour(muted());
            g.setFont(juce::Font(juce::FontOptions(9.5f)));
            g.drawText("AMPLITUDE", plotArea.getX() + 11, plotArea.getY() + 4,
                       plotArea.getWidth() - 22, 13, juce::Justification::centredLeft);
            g.drawText("TWO CYCLES", plotArea.getX() + 11, plotArea.getBottom() - 17,
                       plotArea.getWidth() - 22, 13, juce::Justification::centredRight);

            const auto waveform = processor.getWaveform();
            // Published snapshots remain alive for the processor's lifetime.
            const auto* custom = processor.getCustomWavetableForUI();
            const auto& builtIn = WavetableBank::builtIn().getTable(waveform);
            const auto& table = waveform == Waveform::Custom && custom != nullptr
                ? *custom : builtIn;

            juce::Path path;
            constexpr int segments = 320;
            for (int i = 0; i < segments; ++i)
            {
                const float cycles = 2.0f * static_cast<float>(i)
                    / static_cast<float>(segments - 1);
                const float wrapped = cycles - std::floor(cycles);
                const float position = wrapped * static_cast<float>(table.size());
                const auto i0 = static_cast<std::size_t>(position) % table.size();
                const auto i1 = (i0 + 1) % table.size();
                const float fraction = position - static_cast<float>(i0);
                const float amplitude = table[i0] * (1.0f - fraction) + table[i1] * fraction;
                const float x = plot.getX() + plot.getWidth()
                    * static_cast<float>(i) / static_cast<float>(segments - 1);
                const float y = plot.getCentreY() - amplitude * plot.getHeight() * 0.41f;
                if (i == 0) path.startNewSubPath(x, y);
                else path.lineTo(x, y);
            }
            g.setColour(cyan().withAlpha(0.13f));
            g.strokePath(path, juce::PathStrokeType(6.0f));
            g.setColour(cyan());
            g.strokePath(path, juce::PathStrokeType(2.2f));
        }

        void resized() override
        {
            power.setBounds(getWidth() - 70, 9, 55, 22);
            auto area = getBodyBounds();

            // Sketch order, top to bottom: display, waveform selector,
            // LOAD (+ reserved CV slot), file status, frame slider.
            // Built bottom-up so the display takes all remaining height.
            auto frameRow = area.removeFromBottom(24);
            frameLabel.setBounds(frameRow.removeFromLeft(84));
            frameSlider.setBounds(frameRow);
            area.removeFromBottom(10);

            statusLabel.setBounds(area.removeFromBottom(15));
            fileLabel.setBounds(area.removeFromBottom(16));
            area.removeFromBottom(6);

            constexpr int buttonWidth = 126;
            constexpr int buttonGap = 10;
            auto files = area.removeFromBottom(30)
                             .withSizeKeepingCentre(2 * buttonWidth + buttonGap, 30);
            loadButton.setBounds(files.removeFromLeft(buttonWidth));
            files.removeFromLeft(buttonGap);
            cameraButton.setBounds(files);
            area.removeFromBottom(12);

            auto selector = area.removeFromBottom(32);
            const int cellWidth = selector.getWidth() / static_cast<int>(buttons.size());
            for (std::size_t i = 0; i < buttons.size(); ++i)
            {
                auto cell = i + 1 == buttons.size() ? selector : selector.removeFromLeft(cellWidth);
                buttons[i].setBounds(cell);
            }
            area.removeFromBottom(10);
            plotArea = area;
        }

    private:
        void chooseWav()
        {
            chooser = std::make_unique<juce::FileChooser>(
                "Load a wavetable WAV", juce::File(), "*.wav");
            const auto safe = juce::Component::SafePointer<OscillatorPanel>(this);
            chooser->launchAsync(juce::FileBrowserComponent::openMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                [safe](const juce::FileChooser& selected)
                {
                    if (safe == nullptr) return;
                    const auto file = selected.getResult();
                    if (file == juce::File()) return;
                    juce::String message;
                    const bool ok = safe->processor.loadCustomWav(file, message);
                    safe->refreshSelection();
                    if (!ok)
                    {
                        safe->statusLabel.setText(message, juce::dontSendNotification);
                        safe->statusLabel.setColour(juce::Label::textColourId, orange());
                    }
                    safe->statusLabel.setTooltip(message);
                });
        }

        void timerCallback() override
        {
            const auto newWaveform = processor.getWaveform();
            const auto* newTable = processor.getCustomWavetableForUI();
            if (newWaveform != lastWaveform || newTable != lastTable ||
                processor.getCustomFrameIndex() != lastFrameIndex ||
                processor.getCustomFrameCount() != lastFrameCount)
                refreshSelection();
        }

        void refreshSelection()
        {
            lastWaveform = processor.getWaveform();
            lastTable = processor.getCustomWavetableForUI();
            lastFrameCount = processor.getCustomFrameCount();
            lastFrameIndex = processor.getCustomFrameIndex();

            frameSlider.setRange(0.0, static_cast<double>(juce::jmax(1, lastFrameCount - 1)), 1.0);
            frameSlider.setValue(static_cast<double>(lastFrameIndex), juce::dontSendNotification);
            frameSlider.setEnabled(lastFrameCount > 1 && lastWaveform == Waveform::Custom);
            frameLabel.setText("FRAME " + juce::String(lastFrameCount > 0 ? lastFrameIndex + 1 : 0)
                + "/" + juce::String(lastFrameCount), juce::dontSendNotification);

            buttons[4].setEnabled(lastTable != nullptr);
            for (std::size_t i = 0; i < buttons.size(); ++i)
                buttons[i].setToggleState(static_cast<int>(i) == static_cast<int>(lastWaveform),
                                          juce::dontSendNotification);
            const auto name = processor.getCustomWavetableName();
            fileLabel.setText(name.isEmpty() ? "No custom wavetable" : name,
                              juce::dontSendNotification);
            fileLabel.setTooltip(name);
            if (lastTable != nullptr)
            {
                const bool warning = processor.getCustomSeamWarning();
                statusLabel.setText(warning ? "SEAM WARNING - check loop boundary"
                                             : "Custom WAV ready",
                                    juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId,
                                      warning ? orange() : cyan());
            }
            else
            {
                statusLabel.setText("Load a wavetable WAV to enable CUSTOM",
                                    juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, muted());
            }
            repaint(plotArea);
        }

        ObjectSynthAudioProcessor& processor;
        std::array<juce::TextButton, 5> buttons;
        juce::TextButton loadButton;
        juce::TextButton cameraButton;
        PowerButton power;
        std::unique_ptr<ButtonAttachment> powerAttachment;
        juce::Slider frameSlider;
        juce::Label frameLabel;
        juce::Label fileLabel, statusLabel;
        std::unique_ptr<juce::FileChooser> chooser;
        juce::Rectangle<int> plotArea;
        Waveform lastWaveform = Waveform::Sine;
        const WavetableBank::Table* lastTable = nullptr;
        int lastFrameIndex = -1;
        int lastFrameCount = -1;
    };
}

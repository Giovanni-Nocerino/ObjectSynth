#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"
#include "../CameraPipeline.h"
#include "ModeDial.h"
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

            cameraButton.setButtonText("FROM CAMERA");
            cameraButton.setTooltip("Photograph an object and load the preset made from it "
                                    "(opens the CV pipeline in Terminal). Click again to stop waiting.");
            cameraButton.onClick = [this] { toggleCamera(); };
            cameraButton.getProperties().set(shutterButtonProperty(), true);
            addAndMakeVisible(cameraButton);

            modeDial.onChange = [this](int index)
            {
                if (index == 4 && !processor.hasCustomWavetable()) return;
                processor.setWaveform(static_cast<Waveform>(index));
                refreshSelection();
            };
            modeDial.setTooltip("Waveform: turn the dial, click a mode or use the mouse wheel");
            addAndMakeVisible(modeDial);

            // The frame is chosen by turning the lens ring (circular drag).
            frameSlider.setSliderStyle(juce::Slider::Rotary);
            frameSlider.setRotaryParameters(-ringSweep, ringSweep, true);
            frameSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            frameSlider.setRange(0.0, 1.0, 1.0);
            frameSlider.setMouseClickGrabsKeyboardFocus(false);
            frameSlider.getProperties().set(lensRingProperty(), true);
            frameSlider.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            frameSlider.setTooltip("Turn the lens ring to select the wavetable frame. "
                                   "Sounding notes crossfade to it.");
            frameSlider.onValueChange = [this]
            {
                if (processor.selectCustomFrame(
                        juce::roundToInt(frameSlider.getValue())))
                    refreshSelection();
                repaint(plotArea);
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

            const auto waveform = processor.getWaveform();
            static const char* names[] { "SINE", "SAW", "SQUARE", "TRI", "CUSTOM" };
            juce::String bottom = names[static_cast<int>(waveform)];
            if (waveform == Waveform::Custom && processor.getCustomFrameCount() > 0)
                bottom << "  FRAME " << (processor.getCustomFrameIndex() + 1)
                       << "/" << processor.getCustomFrameCount();
            const bool turnable = frameSlider.isEnabled();
            const auto ringAngle = turnable
                ? -ringSweep + 2.0f * ringSweep * static_cast<float>(frameSlider.valueToProportionOfLength(frameSlider.getValue()))
                : 0.0f;
            const auto glass = drawLens(g, plotArea.toFloat(), "OBJECTSYNTH  WAVETABLE  1:2048",
                                        bottom, ringAngle, turnable);
            const float radius = glass.getWidth() * 0.5f;
            // Two cycles across the glass, inside a centred band.
            const auto plot = glass.withSizeKeepingCentre(radius * 1.72f, radius * 1.15f);

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
            {
                juce::Graphics::ScopedSaveState state(g);
                juce::Path lensClip;
                lensClip.addEllipse(glass);
                g.reduceClipRegion(lensClip);
                g.setColour(cyan().withAlpha(0.13f));
                g.strokePath(path, juce::PathStrokeType(6.0f));
                g.setColour(cyan());
                g.strokePath(path, juce::PathStrokeType(2.2f));
            }
            drawLensGlare(g, glass);
        }

        void resized() override
        {
            power.setBounds(getWidth() - 70, 9, 55, 22);
            auto area = getBodyBounds();

            // Sketch order, top to bottom: display, waveform selector,
            // LOAD (+ reserved CV slot), file status, frame slider.
            // Built bottom-up so the display takes all remaining height.

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

            modeDial.setBounds(area.removeFromBottom(40));
            area.removeFromBottom(10);
            plotArea = area;
            const int lens = juce::jmin(plotArea.getWidth(), plotArea.getHeight());
            frameSlider.setBounds(plotArea.withSizeKeepingCentre(lens, lens));
        }

    private:
        void toggleCamera()
        {
            if (camera.isRunning())
            {
                camera.cancel();
                showCameraState();
                showStatus("Stopped waiting for the camera", false);
                return;
            }
            juce::String message;
            const bool ok = camera.start(message);
            showCameraState();
            showStatus(message, ok);
        }

        void pollCamera()
        {
            CameraPipeline::Result result;
            if (camera.poll(result) != CameraPipeline::State::Finished) return;
            showCameraState();
            if (!result.ok)
            {
                showStatus(result.cancelled ? "Camera closed, no preset" : result.message, false);
                return;
            }
            juce::String message;
            const bool ok = processor.loadPresetFile(result.preset, message);
            refreshSelection();
            showStatus(ok ? "Loaded preset: " + result.preset.getFileNameWithoutExtension()
                          : message, ok);
        }

        void showCameraState()
        {
            cameraButton.setButtonText(camera.isRunning() ? "WAITING..." : "FROM CAMERA");
            cameraButton.setToggleState(camera.isRunning(), juce::dontSendNotification);
        }

        // Overrides the wavetable status until the next selection change.
        void showStatus(const juce::String& message, bool ok)
        {
            statusLabel.setText(message, juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, ok ? cyan() : orange());
            statusLabel.setTooltip(message);
        }

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
            pollCamera();
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

            modeDial.setModeEnabled(4, lastTable != nullptr);
            modeDial.setSelected(static_cast<int>(lastWaveform));
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
        ModeDial modeDial { { "SINE", "SAW", "SQUARE", "TRI", "CUSTOM" } };
        juce::TextButton loadButton;
        juce::TextButton cameraButton;
        CameraPipeline camera;
        PowerButton power;
        std::unique_ptr<ButtonAttachment> powerAttachment;
        static constexpr float ringSweep = 2.4f; // radians each side of the top
        juce::Slider frameSlider;
        juce::Label fileLabel, statusLabel;
        std::unique_ptr<juce::FileChooser> chooser;
        juce::Rectangle<int> plotArea;
        Waveform lastWaveform = Waveform::Sine;
        const WavetableBank::Table* lastTable = nullptr;
        int lastFrameIndex = -1;
        int lastFrameCount = -1;
    };
}

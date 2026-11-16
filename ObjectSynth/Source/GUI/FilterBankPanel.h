#pragma once

#include "Theme.h"
#include <array>
#include <complex>
#include <cmath>

namespace osui
{
    class FilterBankPanel final : public ModulePanel, private juce::Timer
    {
    public:
        explicit FilterBankPanel(APVTS& state, juce::AudioProcessor& owner)
            : ModulePanel("FILTER BANK"),
              processor(owner)
        {
            constexpr const char* ids[3][3] = {
                {"lpCutoff", "lpResonance", "lpMix"},
                {"hpCutoff", "hpResonance", "hpMix"},
                {"bpCutoff", "bpResonance", "bpMix"}
            };
            constexpr const char* names[] = {"CUTOFF", "RESONANCE", "MIX"};
            for (std::size_t i = 0; i < controls.size(); ++i)
                for (std::size_t j = 0; j < controls[i].size(); ++j)
                    setupKnob(*this, controls[i][j], state, ids[i][j], names[j],
                              branchColour(i), j == 0 ? " Hz" : "");

            setupPowerButton(*this, power, powerAttachment, state, "filterBankEnabled");

            // Only the UI timer repaints these graphs; the audio thread never draws.
            startTimerHz(24);
        }

        void paint(juce::Graphics& g) override
        {
            ModulePanel::paint(g);
            drawResponse(g);

            constexpr const char* names[] = {"LOW-PASS", "HIGH-PASS", "BAND-PASS"};
            for (std::size_t i = 0; i < rows.size(); ++i)
            {
                const auto area = rows[i];
                if (area.isEmpty()) continue;

                g.setColour(plotFill().withAlpha(0.82f));
                g.fillRoundedRectangle(area.toFloat(), 7.0f);
                g.setColour(panelEdge().withAlpha(0.75f));
                g.drawRoundedRectangle(area.toFloat(), 7.0f, 1.0f);
                g.setColour(branchColour(i));
                g.fillRoundedRectangle(static_cast<float>(area.getX() + 10),
                                       static_cast<float>(area.getCentreY() - 12),
                                       3.0f, 24.0f, 1.5f);
                g.setFont(11.0f);
                g.drawText(names[i], area.getX() + 18, area.getCentreY() - 12,
                           75, 24, juce::Justification::centredLeft);
            }
        }

        void resized() override
        {
            // Dedicated large plot on the left, all nine knobs in three compact
            // LP/HP/BP rows on the right. The editor dimensions are fixed.
            power.setBounds(getWidth() - 70, 9, 55, 22);
            auto area = getBodyBounds();
            plotArea = area.removeFromLeft(628);
            area.removeFromLeft(12);

            const int rowHeight = area.getHeight() / static_cast<int>(rows.size());
            for (std::size_t i = 0; i < rows.size(); ++i)
            {
                rows[i] = i + 1 == rows.size() ? area : area.removeFromTop(rowHeight);
                auto knobArea = rows[i].reduced(3, 3);
                knobArea.removeFromLeft(93); // label/colour key beside each row

                const int knobWidth = knobArea.getWidth() / 3;
                for (std::size_t j = 0; j < controls[i].size(); ++j)
                {
                    auto cell = j + 1 == controls[i].size()
                                    ? knobArea : knobArea.removeFromLeft(knobWidth);
                    positionKnob(controls[i][j], cell);
                }
            }
        }

    private:
        using Complex = std::complex<double>;

        static juce::Colour branchColour(std::size_t i)
        {
            switch (i)
            {
                case 0: return cyan();
                case 1: return orange();
                default: return purple();
            }
        }

        void timerCallback() override { repaint(plotArea); }

        // Bilinear-transformed, second-order TPT response. This is a modelled
        // transfer function, not an FFT of the audio output.
        static Complex response(std::size_t type, double frequency, double cutoff,
                                double q, double sampleRate)
        {
            const double pi = juce::MathConstants<double>::pi;
            const double f = juce::jlimit(20.0, sampleRate * 0.49, frequency);
            const double fc = juce::jlimit(20.0, sampleRate * 0.45, cutoff);
            const double omega = std::tan(pi * f / sampleRate)
                                 / std::tan(pi * fc / sampleRate);
            const Complex s(0.0, omega);
            const Complex den = 1.0 + s / q + s * s;
            if (type == 0) return 1.0 / den;
            if (type == 1) return (s * s) / den;
            return s / den;
        }

        static float graphY(double magnitude, juce::Rectangle<float> chart)
        {
            const double db = 20.0 * std::log10(juce::jmax(1.0e-7, magnitude));
            // Visible range is +18 to -42 dB.
            const float relative = static_cast<float>((18.0 - juce::jlimit(-42.0, 18.0, db)) / 60.0);
            return chart.getY() + chart.getHeight() * relative;
        }

        static void addPoint(juce::Path& path, float x, Complex value,
                             juce::Rectangle<float> chart, bool first)
        {
            const float y = graphY(std::abs(value), chart);
            if (first) path.startNewSubPath(x, y);
            else       path.lineTo(x, y);
        }

        void drawResponse(juce::Graphics& g)
        {
            if (plotArea.isEmpty()) return;
            const auto container = plotArea.toFloat();
            g.setColour(plotFill());
            g.fillRoundedRectangle(container, 9.0f);
            g.setColour(panelEdge());
            g.drawRoundedRectangle(container, 9.0f, 1.0f);

            // Space for axis labels so curves cannot obscure the tick text.
            const auto chart = container.withTrimmedLeft(48.0f)
                                        .withTrimmedRight(14.0f)
                                        .withTrimmedTop(29.0f)
                                        .withTrimmedBottom(29.0f);
            if (chart.getWidth() < 100.0f || chart.getHeight() < 60.0f) return;

            // Horizontal decibel scale, with a highlighted 0 dB reference.
            constexpr int dbTicks[] = {18, 0, -18, -36};
            for (const int db : dbTicks)
            {
                const float y = chart.getY() + (18.0f - static_cast<float>(db)) / 60.0f
                                                     * chart.getHeight();
                g.setColour(db == 0 ? muted().withAlpha(0.50f)
                                    : panelEdge().withAlpha(0.62f));
                g.drawHorizontalLine(static_cast<int>(y), chart.getX(), chart.getRight());
                g.setColour(muted());
                g.setFont(10.0f);
                g.drawText((db > 0 ? "+" : "") + juce::String(db) + " dB",
                           static_cast<int>(container.getX() + 4), static_cast<int>(y - 9),
                           39, 18, juce::Justification::centredRight);
            }

            // Logarithmic frequency ticks across the 20 Hz .. 20 kHz span.
            constexpr double frequencies[] = {20.0, 100.0, 1000.0, 5000.0, 20000.0};
            constexpr const char* freqText[] = {"20 Hz", "100 Hz", "1 kHz", "5 kHz", "20 kHz"};
            for (std::size_t i = 0; i < std::size(frequencies); ++i)
            {
                const float t = static_cast<float>(std::log10(frequencies[i] / 20.0) / 3.0);
                const float x = chart.getX() + chart.getWidth() * t;
                g.setColour(panelEdge().withAlpha(0.6f));
                g.drawVerticalLine(static_cast<int>(x), chart.getY(), chart.getBottom());
                g.setColour(muted());
                g.setFont(10.0f);
                const auto labelWidth = 59;
                const int labelX = static_cast<int>(x) - (i == 0 ? 0 : i == 4 ? labelWidth : labelWidth / 2);
                g.drawText(freqText[i], labelX,
                           static_cast<int>(chart.getBottom() + 5.0f), labelWidth, 16,
                           i == 0 ? juce::Justification::centredLeft
                                  : i == 4 ? juce::Justification::centredRight
                                           : juce::Justification::centred);
            }

            std::array<double, 3> cutoff {}, resonance {}, mix {};
            for (std::size_t i = 0; i < controls.size(); ++i)
            {
                cutoff[i] = controls[i][0].slider.getValue();
                resonance[i] = controls[i][1].slider.getValue();
                mix[i] = controls[i][2].slider.getValue();
            }

            const double sum = mix[0] + mix[1] + mix[2];
            const double dry = juce::jmax(0.0, 1.0 - sum);
            const double normalization = juce::jmax(1.0, sum);
            const double fs = processor.getSampleRate() > 0.0
                                  ? processor.getSampleRate() : 48000.0;
            constexpr int points = 300;
            std::array<juce::Path, 4> paths;
            for (int i = 0; i < points; ++i)
            {
                const double t = static_cast<double>(i) / (points - 1);
                const double frequency = 20.0 * std::pow(1000.0, t);
                const float x = chart.getX() + chart.getWidth() * static_cast<float>(t);
                Complex combined(dry, 0.0);
                for (std::size_t j = 0; j < 3; ++j)
                {
                    const auto h = response(j, frequency, cutoff[j],
                                            juce::jmax(0.3, resonance[j]), fs);
                    combined += mix[j] * h;
                    addPoint(paths[j], x, h, chart, i == 0);
                }
                addPoint(paths[3], x, combined / normalization, chart, i == 0);
            }

            // Individual branch responses are faded when their Mix is zero.
            for (std::size_t j = 0; j < 3; ++j)
            {
                g.setColour(branchColour(j).withAlpha(mix[j] < 0.005 ? 0.22f : 0.86f));
                g.strokePath(paths[j], juce::PathStrokeType(1.7f));
            }
            g.setColour(text());
            g.strokePath(paths[3], juce::PathStrokeType(2.8f));

            // Compact, in-graph legend. White is the resulting response.
            constexpr const char* legends[] = {"LP", "HP", "BP", "TOTAL"};
            const juce::Colour colours[] = {cyan(), orange(), purple(), text()};
            float legendX = container.getX() + 15.0f;
            for (int i = 0; i < 4; ++i)
            {
                g.setColour(colours[i]);
                g.fillRoundedRectangle(legendX, container.getY() + 10.0f, 11.0f, 3.0f, 1.5f);
                g.setColour(muted());
                g.setFont(10.0f);
                const int textWidth = i == 3 ? 45 : 25;
                g.drawText(legends[i], static_cast<int>(legendX + 15.0f),
                           static_cast<int>(container.getY() + 3.0f), textWidth, 18,
                           juce::Justification::centredLeft);
                legendX += i == 3 ? 78.0f : 56.0f;
            }
        }

        juce::AudioProcessor& processor;
        PowerButton power;
        std::unique_ptr<ButtonAttachment> powerAttachment;
        std::array<std::array<Knob, 3>, 3> controls;
        std::array<juce::Rectangle<int>, 3> rows;
        juce::Rectangle<int> plotArea;
    };
}

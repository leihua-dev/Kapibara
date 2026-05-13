#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "../SynthCore.h"

class SpectrumView : public juce::Component, private juce::Timer
{
  public:
    explicit SpectrumView(synth::SynthCore &c);

    void paint(juce::Graphics &g) override;

  private:
    void timerCallback() override;

    static void drawBandFrame(juce::Graphics &g, juce::Rectangle<int> band, const juce::String &label);
    static void drawRatioGrid(juce::Graphics &g, juce::Rectangle<int> band, float minLogRel, float maxLogRel);
    static float shapedEnv(float x, synth::EnvCurve curve, float eta);
    static float adsrAt(const synth::GlobalAdsrParams &g,
                        const synth::StaticSpectralFrame &f,
                        int partial,
                        float t);
    static void drawPartialAdsr(juce::Graphics &g,
                                juce::Rectangle<int> band,
                                float x0,
                                float w,
                                const synth::StaticSpectralFrame &frame,
                                const synth::GlobalAdsrParams &adsr,
                                int partial,
                                juce::Colour colour);
    static float previewPhase(const synth::StaticSpectralFrame &frame, int partial);
    static void drawPhaseGlyph(juce::Graphics &g,
                               juce::Rectangle<int> band,
                               float x0,
                               float w,
                               float phase,
                               float driftHz,
                               float jitter);
    static float timelinePartialPeak(const synth::SpectralTimeline &timeline, int partial, float fallback);
    static const char *phaseInitName(synth::PhaseInitMode mode);

    synth::SynthCore &core;
};

#include "SpectrumView.h"

#include <algorithm>
#include <cmath>

SpectrumView::SpectrumView(synth::SynthCore &c) : core(c)
{
    startTimerHz(30);
}

void SpectrumView::paint(juce::Graphics &g)
{
    auto frame = core.getFrameSnapshot();
    auto timeline = core.getTimelineSnapshot();
    auto adsr = core.getGlobalAdsr();
    auto area = getLocalBounds().reduced(8);
    g.fillAll(juce::Colour(0xff15181b));
    g.setColour(juce::Colour(0xff2a2d31));
    g.drawRoundedRectangle(area.toFloat(), 4.0f, 1.0f);

    const int N = std::max(1, frame.partialCount);
    const int visibleN = std::min(N, 160);
    const int stride = std::max(1, (int)std::ceil(float(N) / float(visibleN)));
    const int columns = std::max(1, (N + stride - 1) / stride);
    const float w = (float)area.getWidth() / float(columns);

    auto header = area.removeFromTop(18);
    auto footer = area.removeFromBottom(22);
    auto phaseBand = area.removeFromBottom(24);
    auto ampBand = area.removeFromBottom(std::max(52, area.getHeight() / 3));
    auto freqBand = area.removeFromBottom(std::max(42, area.getHeight() / 3));
    auto adsrBand = area;

    drawBandFrame(g, adsrBand, "per-partial ADSR");
    drawBandFrame(g, freqBand, "relative-to-partial-1 position");
    drawBandFrame(g, ampBand, "amp / source energy");
    drawBandFrame(g, phaseBand, "phase");

    const float baseNu = std::max(0.0001f, std::abs(frame.nu[0]));
    float minLogRel = 0.0f;
    float maxLogRel = 0.0f;
    bool firstRel = true;
    for(int i = 0; i < N; i += stride)
    {
        const float rel = std::max(0.0001f, std::abs(frame.nu[i]) / baseNu);
        const float lr = std::log(rel);
        if(firstRel)
        {
            minLogRel = maxLogRel = lr;
            firstRel = false;
        }
        else
        {
            minLogRel = std::min(minLogRel, lr);
            maxLogRel = std::max(maxLogRel, lr);
        }
    }
    if(maxLogRel - minLogRel < 0.001f)
    {
        minLogRel -= 0.5f;
        maxLogRel += 0.5f;
    }

    drawRatioGrid(g, freqBand, minLogRel, maxLogRel);

    for(int i = 0, column = 0; i < N; i += stride, ++column)
    {
        const float a = juce::jlimit(0.0f, 1.0f, frame.amp[i]);
        const float x0 = (float)area.getX() + float(column) * w;
        const float cx = x0 + w * 0.5f;
        const juce::Colour colour = (frame.mu[i] == 0) ? juce::Colours::cornflowerblue
                                    : (frame.mu[i] == 1) ? juce::Colours::lightgreen
                                                         : juce::Colours::orange;
        drawPartialAdsr(g, adsrBand, x0, w, frame, adsr, i, colour.withAlpha(0.78f));

        const float rel = std::max(0.0001f, std::abs(frame.nu[i]) / baseNu);
        const float fy = juce::jmap(std::log(rel), minLogRel, maxLogRel,
                                    (float)freqBand.getBottom() - 5.0f,
                                    (float)freqBand.getY() + 5.0f);
        const float nearestHarmonic = std::max(1.0f, std::round(rel));
        const float centsFromHarmonic = 1200.0f * std::log2(rel / nearestHarmonic);
        const float inharmonic = juce::jlimit(0.0f, 1.0f, std::abs(centsFromHarmonic) / 80.0f);
        const auto dotColour = colour.interpolatedWith(juce::Colours::magenta, inharmonic);

        g.setColour(dotColour.withAlpha(0.95f));
        const float radius = 2.1f + 2.2f * inharmonic;
        g.fillEllipse(cx - radius, fy - radius, radius * 2.0f, radius * 2.0f);
        if(w > 5.0f)
        {
            const float idealY = juce::jmap(std::log(nearestHarmonic), minLogRel, maxLogRel,
                                            (float)freqBand.getBottom() - 5.0f,
                                            (float)freqBand.getY() + 5.0f);
            g.setColour(dotColour.withAlpha(0.28f));
            g.drawLine(cx, idealY, cx, fy, 1.0f);
        }
        if(w > 6.0f && i > 0)
        {
            const float prevRel = std::max(0.0001f, std::abs(frame.nu[std::max(0, i - stride)]) / baseNu);
            const float py = juce::jmap(std::log(prevRel), minLogRel, maxLogRel,
                                        (float)freqBand.getBottom() - 5.0f,
                                        (float)freqBand.getY() + 5.0f);
            g.setColour(dotColour.withAlpha(0.22f));
            g.drawLine(cx - w, py, cx, fy, 1.0f);
        }

        const float energy = timelinePartialPeak(timeline, i, a);
        const float bh = energy * ((float)ampBand.getHeight() - 10.0f);
        g.setColour(colour.withAlpha(0.28f));
        g.fillRect(juce::Rectangle<float>(x0 + 1.0f, (float)ampBand.getBottom() - 5.0f - bh,
                                          std::max(1.0f, w - 2.0f), bh));
        const float bhNow = a * ((float)ampBand.getHeight() - 10.0f);
        g.setColour(colour.withAlpha(0.82f));
        g.fillRect(juce::Rectangle<float>(x0 + std::max(1.0f, w * 0.25f),
                                          (float)ampBand.getBottom() - 5.0f - bhNow,
                                          std::max(1.0f, w * 0.5f), bhNow));

        drawPhaseGlyph(g, phaseBand, x0, w, previewPhase(frame, i),
                       frame.phaseDriftHz[i], frame.phaseJitter[i]);
    }

    g.setColour(juce::Colours::lightgrey);
    g.setFont(11.0f);
    g.drawText("partial inspector: ADSR + position vs partial-1 + harmonic deviation + amp + phase",
               header, juce::Justification::centredLeft);
    juce::String legend = "freqMode = " + juce::String(frame.freqMode == synth::FreqMode::RelativeRatio
                                                           ? "RelativeRatio"
                                                           : "AbsoluteHz")
                          + "    base partial = " + juce::String(baseNu, 3)
                          + "    N = " + juce::String(frame.partialCount)
                          + "    shown = " + juce::String(columns)
                          + (stride > 1 ? (" / stride " + juce::String(stride)) : "")
                          + "    phaseInitMode = " + phaseInitName(frame.phaseInitMode);
    g.drawText(legend, footer, juce::Justification::centredLeft);
}

void SpectrumView::timerCallback()
{
    repaint();
}

void SpectrumView::drawBandFrame(juce::Graphics &g, juce::Rectangle<int> band, const juce::String &label)
{
    g.setColour(juce::Colour(0xff20252a));
    g.fillRect(band);
    g.setColour(juce::Colour(0xff343a40));
    g.drawRect(band);
    g.setColour(juce::Colour(0xff7f8790));
    g.setFont(9.5f);
    g.drawText(label, band.reduced(4, 1), juce::Justification::topLeft);
}

void SpectrumView::drawRatioGrid(juce::Graphics &g, juce::Rectangle<int> band, float minLogRel, float maxLogRel)
{
    g.setFont(9.0f);
    const int maxH = std::min(32, std::max(1, (int)std::ceil(std::exp(maxLogRel))));
    for(int h = 1; h <= maxH; ++h)
    {
        const float lr = std::log((float)h);
        if(lr < minLogRel || lr > maxLogRel)
            continue;
        const float y = juce::jmap(lr, minLogRel, maxLogRel,
                                   (float)band.getBottom() - 5.0f,
                                   (float)band.getY() + 5.0f);
        const bool octave = (h == 1 || h == 2 || h == 4 || h == 8 || h == 16 || h == 32);
        g.setColour(octave ? juce::Colour(0xff697783) : juce::Colour(0xff343a40));
        g.drawHorizontalLine((int)std::round(y), (float)band.getX(), (float)band.getRight());
        if(octave || h <= 8)
        {
            g.setColour(juce::Colour(0xff929aa2));
            g.drawText(juce::String(h) + "x", band.getX() + 4, (int)y - 8, 36, 14,
                       juce::Justification::centredLeft);
        }
    }
}

float SpectrumView::shapedEnv(float x, synth::EnvCurve curve, float eta)
{
    return juce::jlimit(0.0f, 1.0f, synth::envCurveEval(curve, juce::jlimit(0.0f, 1.0f, x), eta));
}

float SpectrumView::adsrAt(const synth::GlobalAdsrParams &g,
                           const synth::StaticSpectralFrame &f,
                           int partial,
                           float t)
{
    const float a = std::max(0.0005f, g.attack * std::max(0.02f, f.attackScale[partial]));
    const float d = std::max(0.0005f, g.decay * std::max(0.02f, f.decayScale[partial]));
    const float s = juce::jlimit(0.0f, 1.0f, g.sustain * f.sustainLevel[partial]);
    const float r = std::max(0.0005f, g.release * std::max(0.02f, f.releaseScale[partial]));
    const float hold = 0.18f;
    const float total = a + d + hold + r;
    const float sec = t * total;
    if(sec < a)
        return shapedEnv(sec / a, g.attackCurve, g.etaA);
    if(sec < a + d)
    {
        const float u = shapedEnv((sec - a) / d, g.decayCurve, g.etaD);
        return 1.0f + (s - 1.0f) * u;
    }
    if(sec < a + d + hold)
        return s;
    const float u = shapedEnv((sec - a - d - hold) / r, g.releaseCurve, g.etaR);
    return s * (1.0f - u);
}

void SpectrumView::drawPartialAdsr(juce::Graphics &g,
                                   juce::Rectangle<int> band,
                                   float x0,
                                   float w,
                                   const synth::StaticSpectralFrame &frame,
                                   const synth::GlobalAdsrParams &adsr,
                                   int partial,
                                   juce::Colour colour)
{
    const float left = x0 + 1.0f;
    const float width = std::max(1.0f, w - 2.0f);
    const float top = (float)band.getY() + 12.0f;
    const float bottom = (float)band.getBottom() - 4.0f;
    const float sustain = juce::jlimit(0.0f, 1.0f, adsr.sustain * frame.sustainLevel[partial]);
    const float decayRatio = juce::jlimit(0.0f, 1.0f, frame.decayScale[partial] / 3.0f);

    if(width < 4.0f)
    {
        const float y0 = juce::jmap(sustain, 0.0f, 1.0f, bottom, top);
        const float y1 = juce::jmap(decayRatio, 0.0f, 1.0f, bottom, top);
        g.setColour(colour.withAlpha(0.55f));
        g.drawLine(left + width * 0.5f, y0, left + width * 0.5f, y1, 1.0f);
        return;
    }

    juce::Path p;
    constexpr int steps = 10;
    for(int k = 0; k <= steps; ++k)
    {
        const float t = float(k) / float(steps);
        const float y = adsrAt(adsr, frame, partial, t);
        const float px = left + t * width;
        const float py = juce::jmap(y, 0.0f, 1.0f, bottom, top);
        if(k == 0) p.startNewSubPath(px, py);
        else       p.lineTo(px, py);
    }
    g.setColour(colour);
    g.strokePath(p, juce::PathStrokeType(width > 7.0f ? 1.35f : 0.85f));
}

float SpectrumView::previewPhase(const synth::StaticSpectralFrame &frame, int partial)
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    float phase = 0.0f;
    switch(frame.phaseInitMode)
    {
        case synth::PhaseInitMode::Zero: phase = 0.0f; break;
        case synth::PhaseInitMode::Locked: phase = frame.phaseLocked[partial]; break;
        case synth::PhaseInitMode::Alternating: phase = (partial & 1) ? juce::MathConstants<float>::pi : 0.0f; break;
        case synth::PhaseInitMode::Random:
        {
            const float u = std::sin(float(partial + 1) * 12.9898f + float(frame.phaseSeed) * 78.233f) * 43758.5453f;
            phase = (u - std::floor(u)) * twoPi;
            break;
        }
    }
    phase = std::fmod(phase, twoPi);
    if(phase < 0.0f) phase += twoPi;
    return phase;
}

void SpectrumView::drawPhaseGlyph(juce::Graphics &g,
                                  juce::Rectangle<int> band,
                                  float x0,
                                  float w,
                                  float phase,
                                  float driftHz,
                                  float jitter)
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    const float u = phase / twoPi;
    const float width = std::max(1.0f, w - 2.0f);
    const float alpha = juce::jlimit(0.35f, 0.95f, 0.45f + std::abs(driftHz) * 0.12f + jitter * 0.5f);
    auto c = juce::Colour::fromHSV(u, 0.75f, 0.92f, alpha);
    g.setColour(c);
    g.fillRect(juce::Rectangle<float>(x0 + 1.0f, (float)band.getY() + 5.0f, width,
                                      (float)band.getHeight() - 10.0f));
    if(width > 6.0f)
    {
        const float cx = x0 + 1.0f + width * 0.5f;
        const float cy = (float)band.getCentreY();
        const float len = std::min(width * 0.42f, (float)band.getHeight() * 0.35f);
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.drawLine(cx, cy, cx + std::cos(phase) * len, cy - std::sin(phase) * len, 1.1f);
    }
}

float SpectrumView::timelinePartialPeak(const synth::SpectralTimeline &timeline, int partial, float fallback)
{
    const int count = std::clamp(timeline.frameCount, 1, synth::kMaxTimelineFrames);
    float peak = fallback;
    for(int f = 0; f < count; ++f)
    {
        const auto &frame = timeline.frames[(size_t)f];
        if(partial < frame.partialCount)
            peak = std::max(peak, juce::jlimit(0.0f, 1.0f, frame.amp[(size_t)partial]));
    }
    return juce::jlimit(0.0f, 1.0f, peak);
}

const char *SpectrumView::phaseInitName(synth::PhaseInitMode mode)
{
    switch(mode)
    {
        case synth::PhaseInitMode::Zero: return "Zero";
        case synth::PhaseInitMode::Random: return "Random";
        case synth::PhaseInitMode::Locked: return "Locked";
        case synth::PhaseInitMode::Alternating: return "Alternating";
    }
    return "Zero";
}

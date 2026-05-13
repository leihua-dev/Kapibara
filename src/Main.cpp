#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "SynthCore.h"
#include "ui/SpectrumView.h"

#include <map>
#include <memory>
#include <array>
#include <vector>

namespace
{
const char *freqShapeName(synth::FreqShape s)
{
    switch(s)
    {
        case synth::FreqShape::Harmonic: return "Harmonic";
        case synth::FreqShape::Linear: return "Linear";
        case synth::FreqShape::Exponential: return "Exponential";
    }
    return "Harmonic";
}
synth::FreqShape stringToFreqShape(const juce::String &s)
{
    if(s == "Linear") return synth::FreqShape::Linear;
    if(s == "Exponential") return synth::FreqShape::Exponential;
    return synth::FreqShape::Harmonic;
}
const char *warpName(synth::WarpMode m)
{
    switch(m)
    {
        case synth::WarpMode::Tilt: return "Tilt";
        case synth::WarpMode::Sym:  return "Sym";
        case synth::WarpMode::Skew: return "Skew";
    }
    return "Tilt";
}
synth::WarpMode stringToWarp(const juce::String &s)
{
    if(s == "Sym") return synth::WarpMode::Sym;
    if(s == "Skew") return synth::WarpMode::Skew;
    return synth::WarpMode::Tilt;
}
const char *phaseInitName(synth::PhaseInitMode m)
{
    switch(m)
    {
        case synth::PhaseInitMode::Zero: return "Zero";
        case synth::PhaseInitMode::Random: return "Random";
        case synth::PhaseInitMode::Locked: return "Locked";
        case synth::PhaseInitMode::Alternating: return "Alternating";
    }
    return "Zero";
}
synth::PhaseInitMode stringToPhaseInit(const juce::String &s)
{
    if(s == "Random") return synth::PhaseInitMode::Random;
    if(s == "Locked") return synth::PhaseInitMode::Locked;
    if(s == "Alternating") return synth::PhaseInitMode::Alternating;
    return synth::PhaseInitMode::Zero;
}
const char *generatorName(synth::GeneratorType t)
{
    switch(t)
    {
        case synth::GeneratorType::DirectPartial: return "DirectPartial";
        case synth::GeneratorType::ModalODE: return "ModalODE";
        case synth::GeneratorType::PDEModal: return "PDEModal";
        case synth::GeneratorType::FunctionalSampleSource: return "FunctionalSampleSource";
        case synth::GeneratorType::SamplePlayback: return "SamplePlayback";
    }
    return "DirectPartial";
}
synth::GeneratorType stringToGenerator(const juce::String &s)
{
    if(s == "ModalODE") return synth::GeneratorType::ModalODE;
    if(s == "PDEModal") return synth::GeneratorType::PDEModal;
    if(s == "FunctionalSampleSource" || s == "SamplePartialSet")
        return synth::GeneratorType::FunctionalSampleSource;
    if(s == "SamplePlayback")
        return synth::GeneratorType::SamplePlayback;
    return synth::GeneratorType::DirectPartial;
}
const char *modalShapeName(synth::ModalShape s)
{
    switch(s)
    {
        case synth::ModalShape::StringClampedClamped: return "String C-C";
        case synth::ModalShape::StringFreeFree: return "String F-F";
        case synth::ModalShape::BarClampedFree: return "Bar C-F";
        case synth::ModalShape::Tube: return "Tube odd";
    }
    return "String C-C";
}
synth::ModalShape stringToModalShape(const juce::String &s)
{
    if(s == "String F-F") return synth::ModalShape::StringFreeFree;
    if(s == "Bar C-F") return synth::ModalShape::BarClampedFree;
    if(s == "Tube odd") return synth::ModalShape::Tube;
    return synth::ModalShape::StringClampedClamped;
}
const char *pdeBodyName(synth::PDEBody b)
{
    switch(b)
    {
        case synth::PDEBody::StiffString: return "StiffString";
        case synth::PDEBody::Plate: return "Plate";
        case synth::PDEBody::Membrane: return "Membrane";
        case synth::PDEBody::Bar: return "Bar";
    }
    return "StiffString";
}
synth::PDEBody stringToPDEBody(const juce::String &s)
{
    if(s == "Plate") return synth::PDEBody::Plate;
    if(s == "Membrane") return synth::PDEBody::Membrane;
    if(s == "Bar") return synth::PDEBody::Bar;
    return synth::PDEBody::StiffString;
}
const char *modSourceName(synth::ModSource s)
{
    switch(s)
    {
        case synth::ModSource::None: return "-";
        case synth::ModSource::Lfo1: return "LFO1";
        case synth::ModSource::Lfo2: return "LFO2";
        case synth::ModSource::Lfo3: return "LFO3";
        case synth::ModSource::Lfo4: return "LFO4";
        case synth::ModSource::Lfo5: return "LFO5";
        case synth::ModSource::Lfo6: return "LFO6";
        case synth::ModSource::Lfo7: return "LFO7";
        case synth::ModSource::Lfo8: return "LFO8";
        case synth::ModSource::Velocity: return "Velocity";
        case synth::ModSource::KeyTrack: return "KeyTrack";
        case synth::ModSource::Random: return "Random";
        case synth::ModSource::Adsr: return "ADSR";
        case synth::ModSource::GeneratorSelf: return "GeneratorSelf";
        case synth::ModSource::Chaos: return "Chaos";
        case synth::ModSource::Shape: return "Shape";
    }
    return "-";
}
synth::ModSource intToModSource(int i)
{
    if(i < 0 || i > (int)synth::ModSource::Shape) return synth::ModSource::None;
    return (synth::ModSource)i;
}
const char *chaosNoiseName(synth::ChaosNoiseType t)
{
    switch(t)
    {
        case synth::ChaosNoiseType::White: return "White";
        case synth::ChaosNoiseType::Smooth: return "Smooth";
        case synth::ChaosNoiseType::Crackle: return "Crackle";
    }
    return "Smooth";
}
synth::ChaosNoiseType stringToChaosNoise(const juce::String &s)
{
    if(s == "White") return synth::ChaosNoiseType::White;
    if(s == "Crackle") return synth::ChaosNoiseType::Crackle;
    return synth::ChaosNoiseType::Smooth;
}
const char *modDestName(synth::ModDestination d)
{
    switch(d)
    {
        case synth::ModDestination::Amp: return "Amp (M_amp)";
        case synth::ModDestination::Freq: return "Freq (M_freq)";
        case synth::ModDestination::Phase: return "Phase (d_phi)";
        case synth::ModDestination::DecayTime: return "Decay (T_D)";
    }
    return "Amp (M_amp)";
}
const char *weightName(synth::WeightMode w)
{
    switch(w)
    {
        case synth::WeightMode::All: return "All";
        case synth::WeightMode::LowPartials: return "Low (1-x)";
        case synth::WeightMode::HighPartials: return "High (x)";
        case synth::WeightMode::GroupLow: return "mu=0";
        case synth::WeightMode::GroupMid: return "mu=1";
        case synth::WeightMode::GroupHigh: return "mu=2";
        case synth::WeightMode::BandIndex: return "Band[lo,hi)";
    }
    return "All";
}
const char *envCurveName(synth::EnvCurve c)
{
    switch(c)
    {
        case synth::EnvCurve::Exp: return "Exp";
        case synth::EnvCurve::Power: return "Power";
        case synth::EnvCurve::Sigmoid: return "Sigmoid";
    }
    return "Exp";
}
synth::EnvCurve stringToEnvCurve(const juce::String &s)
{
    if(s == "Power") return synth::EnvCurve::Power;
    if(s == "Sigmoid") return synth::EnvCurve::Sigmoid;
    return synth::EnvCurve::Exp;
}
const char *lfoShapeName(synth::LfoShape s)
{
    switch(s)
    {
        case synth::LfoShape::Asymmetric: return "Asymm";
        case synth::LfoShape::Sine: return "Sine";
        case synth::LfoShape::Square: return "Square";
        case synth::LfoShape::Triangle: return "Tri";
        case synth::LfoShape::SampleHold: return "S/H";
    }
    return "Asymm";
}
synth::LfoShape stringToLfoShape(const juce::String &s)
{
    if(s == "Sine") return synth::LfoShape::Sine;
    if(s == "Square") return synth::LfoShape::Square;
    if(s == "Tri") return synth::LfoShape::Triangle;
    if(s == "S/H") return synth::LfoShape::SampleHold;
    return synth::LfoShape::Asymmetric;
}
const char *effectModeUiName(synth::EffectProcessMode m)
{
    return synth::effectModeName(m);
}
synth::EffectProcessMode stringToEffectMode(const juce::String &s)
{
    if(s == "Linear") return synth::EffectProcessMode::Linear;
    if(s == "Nonlinear") return synth::EffectProcessMode::Nonlinear;
    return synth::EffectProcessMode::Normal;
}
const char *filterTypeUiName(synth::FilterType t)
{
    return synth::filterTypeName(t);
}
synth::FilterType stringToFilterType(const juce::String &s)
{
    if(s == "HighPass") return synth::FilterType::HighPass;
    if(s == "BandPass") return synth::FilterType::BandPass;
    return synth::FilterType::LowPass;
}
const char *operatorTypeStr(synth::OperatorType t) { return synth::operatorTypeName(t); }
} // namespace

// =============================================================================
// Compact curve previews
// =============================================================================
class LfoCurveView : public juce::Component, private juce::Timer
{
  public:
    LfoCurveView(synth::SynthCore &c, int lfoIndex) : core(c), index(lfoIndex) { startTimerHz(20); }

    void paint(juce::Graphics &g) override
    {
        auto p = core.getLfoParams(index);
        auto area = getLocalBounds().reduced(4);
        g.fillAll(juce::Colour(0xff121518));
        g.setColour(juce::Colour(0xff2e3338));
        g.drawRoundedRectangle(area.toFloat(), 4.0f, 1.0f);

        auto graph = area.reduced(6);
        g.setColour(juce::Colour(0xff3a4046));
        g.drawHorizontalLine(graph.getCentreY(), (float)graph.getX(), (float)graph.getRight());

        juce::Path path;
        const int w = std::max(2, graph.getWidth());
        for(int x = 0; x < w; ++x)
        {
            const float xi = float(x) / float(w - 1);
            const float y = evalLfo(p, xi);
            const float px = float(graph.getX() + x);
            const float py = juce::jmap(y, -1.0f, 1.0f, float(graph.getBottom()), float(graph.getY()));
            if(x == 0)
                path.startNewSubPath(px, py);
            else
                path.lineTo(px, py);
        }
        g.setColour(p.enabled ? juce::Colours::aqua : juce::Colours::grey);
        g.strokePath(path, juce::PathStrokeType(1.8f));
    }

  private:
    static float evalLfo(const synth::LfoParams &p, float xi)
    {
        auto frac = [](float v) { return v - std::floor(v); };
        xi = frac(xi + p.phase0);
        switch(p.shape)
        {
            case synth::LfoShape::Asymmetric:
            {
                const float rho = juce::jlimit(0.001f, 0.999f, p.rhoLfo);
                const float pu = std::max(0.05f, p.pUp);
                const float pd = std::max(0.05f, p.pDown);
                return xi < rho
                           ? -1.0f + 2.0f * std::pow(xi / rho, pu)
                           : 1.0f - 2.0f * std::pow((xi - rho) / (1.0f - rho), pd);
            }
            case synth::LfoShape::Sine:     return std::sin(juce::MathConstants<float>::twoPi * xi);
            case synth::LfoShape::Square:   return xi < 0.5f ? 1.0f : -1.0f;
            case synth::LfoShape::Triangle: return xi < 0.5f ? (4.0f * xi - 1.0f) : (3.0f - 4.0f * xi);
            case synth::LfoShape::SampleHold:
            {
                const float bucket = std::floor(xi * 12.0f);
                const float u = std::sin(12.9898f * (bucket + p.phase0)) * 43758.5453f;
                return (frac(u) * 2.0f - 1.0f);
            }
        }
        return 0.0f;
    }

    void timerCallback() override { repaint(); }

    synth::SynthCore &core;
    int index = 0;
};

class AdsrCurveView : public juce::Component, private juce::Timer
{
  public:
    AdsrCurveView(synth::SynthCore &c) : core(c) { startTimerHz(20); }

    void paint(juce::Graphics &g) override
    {
        auto p = core.getGlobalAdsr();
        auto area = getLocalBounds().reduced(6);
        g.fillAll(juce::Colour(0xff121518));
        g.setColour(juce::Colour(0xff2e3338));
        g.drawRoundedRectangle(area.toFloat(), 4.0f, 1.0f);

        auto graph = area.reduced(8);
        g.setColour(juce::Colour(0xff3a4046));
        g.drawHorizontalLine(graph.getBottom() - int(p.sustain * graph.getHeight()),
                             (float)graph.getX(), (float)graph.getRight());

        const float total = std::max(0.001f, p.attack + p.decay + p.release + 0.25f);
        const float sustainHold = 0.25f;
        juce::Path path;
        const int w = std::max(2, graph.getWidth());
        for(int x = 0; x < w; ++x)
        {
            const float t = total * float(x) / float(w - 1);
            float y = 0.0f;
            if(t < p.attack)
                y = synth::envCurveEval(p.attackCurve, t / std::max(0.0001f, p.attack), p.etaA);
            else if(t < p.attack + p.decay)
            {
                const float tau = (t - p.attack) / std::max(0.0001f, p.decay);
                y = p.sustain + (1.0f - p.sustain) * (1.0f - synth::envCurveEval(p.decayCurve, tau, p.etaD));
            }
            else if(t < p.attack + p.decay + sustainHold)
                y = p.sustain;
            else
            {
                const float tau = (t - p.attack - p.decay - sustainHold) / std::max(0.0001f, p.release);
                y = p.sustain * (1.0f - synth::envCurveEval(p.releaseCurve, tau, p.etaR));
            }

            const float px = float(graph.getX() + x);
            const float py = juce::jmap(juce::jlimit(0.0f, 1.0f, y), 0.0f, 1.0f,
                                        float(graph.getBottom()), float(graph.getY()));
            if(x == 0)
                path.startNewSubPath(px, py);
            else
                path.lineTo(px, py);
        }
        g.setColour(juce::Colours::lightgreen);
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }

  private:
    void timerCallback() override { repaint(); }

    synth::SynthCore &core;
};

// =============================================================================
// Helper: knob with caption; caption shows "label (variable)"
// =============================================================================
class KnobWithLabel : public juce::Component
{
  public:
    juce::Slider slider;
    juce::Label label;
    KnobWithLabel(const juce::String &caption, double minV, double maxV, double step)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        slider.setRange(minV, maxV, step);
        addAndMakeVisible(slider);
        label.setText(caption, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        label.setFont(juce::Font(juce::FontOptions(12.0f)));
        addAndMakeVisible(label);
    }
    void resized() override
    {
        auto a = getLocalBounds();
        label.setBounds(a.removeFromTop(18));
        slider.setBounds(a);
    }
};

class ComboWithLabel : public juce::Component
{
  public:
    juce::ComboBox combo;
    juce::Label label;
    ComboWithLabel(const juce::String &caption)
    {
        addAndMakeVisible(combo);
        label.setText(caption, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        label.setFont(juce::Font(juce::FontOptions(12.0f)));
        addAndMakeVisible(label);
    }
    void resized() override
    {
        auto a = getLocalBounds();
        label.setBounds(a.removeFromTop(18));
        combo.setBounds(a.reduced(0, 4).removeFromTop(28));
    }
};

// =============================================================================
// Compact Performance controls embedded in Generator view
// =============================================================================
class CompactPerformancePanel : public juce::Component, private juce::Timer
{
  public:
    CompactPerformancePanel(synth::SynthCore &c) : core(c), adsrCurve(c)
    {
        addAndMakeVisible(adsrCurve);

        auto add = [this](std::unique_ptr<KnobWithLabel> &slot, const char *cap, double mn, double mx, double st) {
            slot = std::make_unique<KnobWithLabel>(cap, mn, mx, st);
            addAndMakeVisible(*slot);
        };
        add(attack, "T_A attack", 0.001, 4.0, 0.001);
        add(decay, "T_D decay", 0.005, 4.0, 0.001);
        add(sustain, "S sustain", 0.0, 1.0, 0.001);
        add(release, "T_R release", 0.005, 8.0, 0.001);
        add(etaA, "eta_A", 0.5, 16.0, 0.01);
        add(etaD, "eta_D", 0.5, 16.0, 0.01);
        add(etaR, "eta_R", 0.5, 16.0, 0.01);
        add(gain, "globalGain", 0.0, 1.0, 0.001);

        modeA = std::make_unique<ComboWithLabel>("modeA");
        modeD = std::make_unique<ComboWithLabel>("modeD");
        modeR = std::make_unique<ComboWithLabel>("modeR");
        for(auto *cw : {modeA.get(), modeD.get(), modeR.get()})
        {
            cw->combo.addItem("Exp", 1);
            cw->combo.addItem("Power", 2);
            cw->combo.addItem("Sigmoid", 3);
            addAndMakeVisible(*cw);
        }

        voiceLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        addAndMakeVisible(voiceLabel);

        wireCallbacks();
        syncFromCore();
        startTimerHz(15);
    }

    void refreshFromCore() { syncFromCore(); }

    void resized() override
    {
        auto a = getLocalBounds().reduced(8);
        adsrCurve.setBounds(a.removeFromTop(96).reduced(2));

        auto r1 = a.removeFromTop(96);
        int w1 = r1.getWidth() / 4;
        attack->setBounds(r1.removeFromLeft(w1).reduced(3));
        decay->setBounds(r1.removeFromLeft(w1).reduced(3));
        sustain->setBounds(r1.removeFromLeft(w1).reduced(3));
        release->setBounds(r1.reduced(3));

        auto r2 = a.removeFromTop(96);
        int w2 = r2.getWidth() / 7;
        modeA->setBounds(r2.removeFromLeft(w2).reduced(3));
        etaA->setBounds(r2.removeFromLeft(w2).reduced(3));
        modeD->setBounds(r2.removeFromLeft(w2).reduced(3));
        etaD->setBounds(r2.removeFromLeft(w2).reduced(3));
        modeR->setBounds(r2.removeFromLeft(w2).reduced(3));
        etaR->setBounds(r2.removeFromLeft(w2).reduced(3));
        gain->setBounds(r2.reduced(3));

        voiceLabel.setBounds(a.removeFromTop(24).reduced(4, 0));
    }

  private:
    void timerCallback() override
    {
        voiceLabel.setText("Voices: " + juce::String(core.getActiveVoiceCount()) + "/" + juce::String(synth::kMaxVoices),
                           juce::dontSendNotification);
    }

    void syncFromCore()
    {
        suspend = true;
        auto a = core.getGlobalAdsr();
        attack->slider.setValue(a.attack, juce::dontSendNotification);
        decay->slider.setValue(a.decay, juce::dontSendNotification);
        sustain->slider.setValue(a.sustain, juce::dontSendNotification);
        release->slider.setValue(a.release, juce::dontSendNotification);
        etaA->slider.setValue(a.etaA, juce::dontSendNotification);
        etaD->slider.setValue(a.etaD, juce::dontSendNotification);
        etaR->slider.setValue(a.etaR, juce::dontSendNotification);
        modeA->combo.setSelectedId((int)a.attackCurve + 1, juce::dontSendNotification);
        modeD->combo.setSelectedId((int)a.decayCurve + 1, juce::dontSendNotification);
        modeR->combo.setSelectedId((int)a.releaseCurve + 1, juce::dontSendNotification);
        gain->slider.setValue(core.getGlobalGain(), juce::dontSendNotification);
        suspend = false;
    }

    void wireCallbacks()
    {
        auto pushAdsr = [this] {
            if(suspend) return;
            synth::GlobalAdsrParams a;
            a.attack = (float)attack->slider.getValue();
            a.decay = (float)decay->slider.getValue();
            a.sustain = (float)sustain->slider.getValue();
            a.release = (float)release->slider.getValue();
            a.etaA = (float)etaA->slider.getValue();
            a.etaD = (float)etaD->slider.getValue();
            a.etaR = (float)etaR->slider.getValue();
            a.attackCurve = (synth::EnvCurve)(modeA->combo.getSelectedId() - 1);
            a.decayCurve = (synth::EnvCurve)(modeD->combo.getSelectedId() - 1);
            a.releaseCurve = (synth::EnvCurve)(modeR->combo.getSelectedId() - 1);
            core.setGlobalAdsr(a);
        };
        for(auto *k : {attack.get(), decay.get(), sustain.get(), release.get(),
                        etaA.get(), etaD.get(), etaR.get()})
            k->slider.onValueChange = pushAdsr;
        for(auto *cw : {modeA.get(), modeD.get(), modeR.get()})
            cw->combo.onChange = pushAdsr;
        gain->slider.onValueChange = [this] {
            if(suspend) return;
            core.setGlobalGain((float)gain->slider.getValue());
        };
    }

    synth::SynthCore &core;
    AdsrCurveView adsrCurve;
    std::unique_ptr<KnobWithLabel> attack, decay, sustain, release;
    std::unique_ptr<KnobWithLabel> etaA, etaD, etaR, gain;
    std::unique_ptr<ComboWithLabel> modeA, modeD, modeR;
    juce::Label voiceLabel;
    bool suspend = false;
};

// =============================================================================
// Generator panel - switches sub-panel based on generator type.
// =============================================================================
class GeneratorPanel : public juce::Component
{
  public:
    GeneratorPanel(synth::SynthCore &c) : core(c), spectrumView(c), performancePanel(c)
    {
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&content, false);
        viewport.setScrollBarsShown(true, false);
        content.addAndMakeVisible(spectrumView);
        content.addAndMakeVisible(performancePanel);

        typeCombo.combo.addItem("DirectPartial", 1);
        typeCombo.combo.addItem("ModalODE", 2);
        typeCombo.combo.addItem("PDEModal", 3);
        typeCombo.combo.addItem("FunctionalSampleSource", 4);
        typeCombo.combo.addItem("SamplePlayback", 5);
        content.addAndMakeVisible(typeCombo);

        // DirectPartial seed + shared Generator Preprocessor knobs
        auto add = [this](std::unique_ptr<KnobWithLabel> &slot, const char *cap, double mn, double mx, double st) {
            slot = std::make_unique<KnobWithLabel>(cap, mn, mx, st);
            content.addAndMakeVisible(*slot);
        };
        add(dpPartialCount, "N (partialCount)", 1, synth::kMaxPartials, 1);
        add(dpInharmonic, "Inharm (alpha/k)", 0.0, 1.0, 0.001);
        add(dpTilt, "tilt (-tilt*ln n)", -2.0, 2.0, 0.001);
        add(dpLambda, "lambda (decay)", -2.0, 2.0, 0.001);
        add(dpGamma, "gamma (curve)", 0.25, 4.0, 0.001);
        add(dpFocusCenter, "c_f (center)", 0.0, 1.0, 0.001);
        add(dpFocusWidth, "sigma_f (width)", 0.05, 0.5, 0.001);
        add(dpFocusAmount, "a_f (focus)", -2.0, 2.0, 0.001);
        add(dpWarpAmount, "a_w (warp)", -1.0, 1.0, 0.001);
        add(dpWarpP, "p_t/p_s/p_k", 0.5, 6.0, 0.001);
        add(dpWarpCenter, "c_s (sym center)", 0.0, 1.0, 0.001);
        add(dpAmpRandom, "a_r (random)", 0.0, 1.0, 0.001);
        add(dpDecaySpread, "decayScale spread", 0.0, 1.0, 0.001);
        add(dpReleaseSpread, "releaseScale spread", 0.0, 1.0, 0.001);

        freqShapeCombo = std::make_unique<ComboWithLabel>("freqShape");
        freqShapeCombo->combo.addItem("Harmonic", 1);
        freqShapeCombo->combo.addItem("Linear", 2);
        freqShapeCombo->combo.addItem("Exponential", 3);
        content.addAndMakeVisible(*freqShapeCombo);

        partialSetButton.setButtonText("Load sample (full length)...");
        content.addAndMakeVisible(partialSetButton);
        partialSetLabel.setText("(no sample loaded)", juce::dontSendNotification);
        partialSetLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
        content.addAndMakeVisible(partialSetLabel);

        warpModeCombo = std::make_unique<ComboWithLabel>("warpMode");
        warpModeCombo->combo.addItem("Tilt", 1);
        warpModeCombo->combo.addItem("Sym", 2);
        warpModeCombo->combo.addItem("Skew", 3);
        content.addAndMakeVisible(*warpModeCombo);

        phaseInitCombo = std::make_unique<ComboWithLabel>("phaseInitMode");
        phaseInitCombo->combo.addItem("Zero", 1);
        phaseInitCombo->combo.addItem("Random", 2);
        phaseInitCombo->combo.addItem("Locked", 3);
        phaseInitCombo->combo.addItem("Alternating", 4);
        content.addAndMakeVisible(*phaseInitCombo);

        // ModalODE / PDE knobs (shared)
        add(mPartialCount, "N (modal)", 1, synth::kMaxPartials, 1);
        add(mStiffness, "B stiffness", 0.0, 0.01, 0.00001);
        add(mDampingZeta, "zeta_i base", 0.0, 0.05, 0.0001);
        add(mDampingHF, "zeta HF mult", 0.0, 2.0, 0.001);
        add(mPickup, "x_o pickup", 0.0, 1.0, 0.001);
        modalShapeCombo = std::make_unique<ComboWithLabel>("modal shape");
        modalShapeCombo->combo.addItem("String C-C", 1);
        modalShapeCombo->combo.addItem("String F-F", 2);
        modalShapeCombo->combo.addItem("Bar C-F", 3);
        modalShapeCombo->combo.addItem("Tube odd", 4);
        content.addAndMakeVisible(*modalShapeCombo);

        add(pdePartialCount, "N (pde)", 1, synth::kMaxPartials, 1);
        add(pdeParam1, "param1 (B / inharm)", 0.0, 0.01, 0.00001);
        add(pdeParam2, "param2 (Lx/Ly)", 0.5, 4.0, 0.001);
        add(pdeDamping, "zeta", 0.0, 0.05, 0.0001);
        add(pdeDampingHF, "zeta HF", 0.0, 2.0, 0.001);
        add(pdePickup, "x_o", 0.0, 1.0, 0.001);
        pdeBodyCombo = std::make_unique<ComboWithLabel>("body");
        pdeBodyCombo->combo.addItem("StiffString", 1);
        pdeBodyCombo->combo.addItem("Plate", 2);
        pdeBodyCombo->combo.addItem("Membrane", 3);
        pdeBodyCombo->combo.addItem("Bar", 4);
        content.addAndMakeVisible(*pdeBodyCombo);

        // FunctionalSampleSource: partial count + macro knobs + quality combo
        // + optional user root lock. The macros multiply basis weights at bake
        // time, so they update without re-running the (expensive) analyzer.
        add(saPartialCount, "N partials", 1, synth::kMaxPartials, 1);
        add(fssAttackSharp, "Attack", 0.0, 4.0, 0.01);
        add(fssBrightness,  "Brightness", 0.0, 4.0, 0.01);
        add(fssBody,        "Body", 0.0, 4.0, 0.01);
        add(fssRootHz,      "Root Hz", 20.0, 8000.0, 0.1);

        fssQualityCombo = std::make_unique<ComboWithLabel>("Quality");
        fssQualityCombo->combo.addItem("Draft", 1);
        fssQualityCombo->combo.addItem("Standard", 2);
        fssQualityCombo->combo.addItem("High", 3);
        content.addAndMakeVisible(*fssQualityCombo);

        fssRootLock.setButtonText("Lock root");
        content.addAndMakeVisible(fssRootLock);

        samplePlaybackButton.setButtonText("Load playable sample...");
        content.addAndMakeVisible(samplePlaybackButton);
        samplePlaybackLabel.setText("(no playback sample)", juce::dontSendNotification);
        samplePlaybackLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
        content.addAndMakeVisible(samplePlaybackLabel);
        add(sampleRootMidi, "Root MIDI", 0.0, 127.0, 1.0);
        add(sampleStart, "Start", 0.0, 0.99, 0.001);
        add(sampleEnd, "End", 0.01, 1.0, 0.001);
        add(sampleGain, "Gain", 0.0, 2.0, 0.001);
        add(samplePitch, "Pitch semi", -24.0, 24.0, 0.01);
        add(sampleAttack, "Attack ms", 0.0, 500.0, 1.0);
        add(sampleRelease, "Release ms", 1.0, 2000.0, 1.0);
        sampleLoop.setButtonText("Loop");
        sampleReverse.setButtonText("Reverse");
        content.addAndMakeVisible(sampleLoop);
        content.addAndMakeVisible(sampleReverse);

        // Unison controls (shared across all generator types).
        add(unisonVoices, "Unison voices", 1, synth::kMaxUnison, 1);
        add(unisonDetune, "detune cents", 0.0, 50.0, 0.1);
        add(unisonWidth, "stereo width", 0.0, 1.0, 0.001);
        add(unisonPhase, "phase spread", 0.0, 1.0, 0.001);

        // Partial-count Max helper. partialMaxRefHz is the f0 used to compute
        // the largest sensible N (so N * refHz <= 20 kHz).
        add(partialMaxRefHz, "Max@refHz", 20.0, 8000.0, 0.5);
        partialMaxButton.setButtonText("Set N = Max");
        content.addAndMakeVisible(partialMaxButton);
        partialMaxLabel.setText("Max N = ?", juce::dontSendNotification);
        partialMaxLabel.setColour(juce::Label::textColourId, juce::Colours::lightyellow);
        partialMaxLabel.setJustificationType(juce::Justification::centred);
        content.addAndMakeVisible(partialMaxLabel);

        wireCallbacks();
        syncFromCore();
        updateVisibility();
    }

    void resized() override
    {
        viewport.setBounds(getLocalBounds());
        const int contentW = std::max(900, viewport.getWidth() - 18);
        const int contentH = 150 + 54 + 34 + 4 * 100 + 315 + 40;
        content.setSize(contentW, contentH);
        auto a = content.getLocalBounds().reduced(10);
        spectrumView.setBounds(a.removeFromTop(150).reduced(0, 4));
        typeCombo.setBounds(a.removeFromTop(54).removeFromLeft(220));
        auto partialFileRow = a.removeFromTop(34);
        partialSetButton.setBounds(partialFileRow.removeFromLeft(150).reduced(2));
        partialFileRow.removeFromLeft(8);
        partialSetLabel.setBounds(partialFileRow.reduced(2));

        auto knobRowH = 100;
        auto k1 = a.removeFromTop(knobRowH);
        auto k2 = a.removeFromTop(knobRowH);
        auto k3 = a.removeFromTop(knobRowH);
        auto kU = a.removeFromTop(knobRowH); // unison + partial-max row

        auto layoutRow = [](juce::Rectangle<int> row, std::vector<juce::Component *> items) {
            if(items.empty()) return;
            int w = row.getWidth() / (int)items.size();
            for(auto *c : items)
            {
                c->setBounds(row.removeFromLeft(w).reduced(4));
            }
        };

        if(visibleType == synth::GeneratorType::DirectPartial)
        {
            layoutRow(k1, {dpPartialCount.get(), freqShapeCombo.get(), dpInharmonic.get(),
                           dpTilt.get(), dpLambda.get(), dpGamma.get()});
            layoutRow(k2, {warpModeCombo.get(), dpWarpAmount.get(), dpWarpP.get(),
                           dpWarpCenter.get(), dpFocusCenter.get(), dpFocusWidth.get()});
            layoutRow(k3, {dpFocusAmount.get(), dpAmpRandom.get(), dpDecaySpread.get(),
                           dpReleaseSpread.get(), phaseInitCombo.get()});
        }
        else if(visibleType == synth::GeneratorType::ModalODE)
        {
            layoutRow(k1, {mPartialCount.get(), modalShapeCombo.get(), mStiffness.get(),
                           mDampingZeta.get(), mDampingHF.get(), mPickup.get()});
            layoutRow(k2, {freqShapeCombo.get(), dpInharmonic.get(), dpTilt.get(),
                           dpLambda.get(), dpGamma.get(), warpModeCombo.get()});
            layoutRow(k3, {dpWarpAmount.get(), dpWarpP.get(), dpWarpCenter.get(),
                           dpFocusCenter.get(), dpFocusWidth.get(), dpFocusAmount.get(),
                           dpAmpRandom.get(), dpDecaySpread.get(), dpReleaseSpread.get(),
                           phaseInitCombo.get()});
        }
        else if(visibleType == synth::GeneratorType::PDEModal)
        {
            layoutRow(k1, {pdePartialCount.get(), pdeBodyCombo.get(), pdeParam1.get(),
                           pdeParam2.get(), pdeDamping.get(), pdeDampingHF.get()});
            layoutRow(k2, {pdePickup.get(), freqShapeCombo.get(), dpInharmonic.get(),
                           dpTilt.get(), dpLambda.get(), dpGamma.get()});
            layoutRow(k3, {warpModeCombo.get(), dpWarpAmount.get(), dpWarpP.get(),
                           dpWarpCenter.get(), dpFocusCenter.get(), dpFocusWidth.get(),
                           dpFocusAmount.get(), dpAmpRandom.get(), dpDecaySpread.get(),
                           dpReleaseSpread.get(), phaseInitCombo.get()});
        }
        else if(visibleType == synth::GeneratorType::FunctionalSampleSource)
        {
            // FunctionalSampleSource: macros (Attack/Brightness/Body) sit on
            // row 1 with the partial count and quality combo; row 2 hosts the
            // file loader + root lock + root override knob; row 3 keeps the
            // shared frequency-shape strip available for editing.
            layoutRow(k1, {saPartialCount.get(), fssAttackSharp.get(),
                           fssBrightness.get(), fssBody.get(), fssQualityCombo.get()});
            auto br = k2.reduced(8);
            partialSetButton.setBounds(br.removeFromLeft(190).removeFromTop(28));
            br.removeFromLeft(8);
            partialSetLabel.setBounds(br.removeFromLeft(260).removeFromTop(28));
            br.removeFromLeft(8);
            fssRootLock.setBounds(br.removeFromLeft(100).removeFromTop(28));
            br.removeFromLeft(8);
            fssRootHz->setBounds(br.removeFromLeft(120).removeFromTop(64));
            layoutRow(k3, {freqShapeCombo.get(), dpInharmonic.get(), dpTilt.get(),
                           warpModeCombo.get(), dpWarpAmount.get(), dpWarpP.get()});
        }
        else
        {
            auto br = k1.reduced(8);
            samplePlaybackButton.setBounds(br.removeFromLeft(190).removeFromTop(28));
            br.removeFromLeft(8);
            samplePlaybackLabel.setBounds(br.removeFromLeft(360).removeFromTop(28));
            sampleLoop.setBounds(br.removeFromLeft(80).removeFromTop(28));
            sampleReverse.setBounds(br.removeFromLeft(90).removeFromTop(28));
            layoutRow(k2, {sampleRootMidi.get(), sampleStart.get(), sampleEnd.get(),
                           sampleGain.get(), samplePitch.get()});
            layoutRow(k3, {sampleAttack.get(), sampleRelease.get()});
        }

        // Shared row: unison + partial-count Max helper (visible for every generator).
        {
            auto unisonArea = kU.removeFromLeft(kU.getWidth() / 2);
            int wu = unisonArea.getWidth() / 4;
            unisonVoices->setBounds(unisonArea.removeFromLeft(wu).reduced(3));
            unisonDetune->setBounds(unisonArea.removeFromLeft(wu).reduced(3));
            unisonWidth->setBounds(unisonArea.removeFromLeft(wu).reduced(3));
            unisonPhase->setBounds(unisonArea.reduced(3));

            int wm = kU.getWidth() / 3;
            partialMaxRefHz->setBounds(kU.removeFromLeft(wm).reduced(3));
            auto btnCol = kU.removeFromLeft(wm).reduced(8);
            partialMaxButton.setBounds(btnCol.removeFromTop(34));
            partialMaxLabel.setBounds(btnCol.reduced(0, 8));
        }

        performancePanel.setBounds(a.removeFromTop(300).reduced(0, 8));
    }

    void syncFromCore()
    {
        suspendCallbacks = true;
        performancePanel.refreshFromCore();
        auto p = core.getGeneratorParams();
        typeCombo.combo.setText(generatorName(p.type), juce::dontSendNotification);

        auto &d = p.direct;
        auto &pre = p.pre;
        dpPartialCount->slider.setValue(d.partialCount, juce::dontSendNotification);
        freqShapeCombo->combo.setText(freqShapeName(pre.freqShape), juce::dontSendNotification);
        const auto &fss = p.functionalSource;
        partialSetLabel.setText(fss.filePath.empty() ? "(no sample loaded)"
                                                    : juce::String(fss.filePath),
                                  juce::dontSendNotification);
        fssAttackSharp->slider.setValue(fss.attackSharpness, juce::dontSendNotification);
        fssBrightness ->slider.setValue(fss.brightnessDecay, juce::dontSendNotification);
        fssBody       ->slider.setValue(fss.bodyResonance,   juce::dontSendNotification);
        fssRootHz     ->slider.setValue(fss.userRootHz,      juce::dontSendNotification);
        fssRootLock.setToggleState(fss.userLockRoot, juce::dontSendNotification);
        fssQualityCombo->combo.setSelectedId(int(fss.quality) + 1, juce::dontSendNotification);
        const auto &sp = p.samplePlayback;
        samplePlaybackLabel.setText(sp.filePath.empty() ? "(no playback sample)" : juce::String(sp.filePath),
                                    juce::dontSendNotification);
        sampleRootMidi->slider.setValue(sp.rootMidi, juce::dontSendNotification);
        sampleStart->slider.setValue(sp.start01, juce::dontSendNotification);
        sampleEnd->slider.setValue(sp.end01, juce::dontSendNotification);
        sampleGain->slider.setValue(sp.playbackGain, juce::dontSendNotification);
        samplePitch->slider.setValue(sp.pitchOffsetSemitones, juce::dontSendNotification);
        sampleAttack->slider.setValue(sp.attackMs, juce::dontSendNotification);
        sampleRelease->slider.setValue(sp.releaseMs, juce::dontSendNotification);
        sampleLoop.setToggleState(sp.loopEnabled, juce::dontSendNotification);
        sampleReverse.setToggleState(sp.reverse, juce::dontSendNotification);
        dpInharmonic->slider.setValue(pre.inharmonicAmount, juce::dontSendNotification);
        dpTilt->slider.setValue(pre.tilt, juce::dontSendNotification);
        dpLambda->slider.setValue(pre.lambda, juce::dontSendNotification);
        dpGamma->slider.setValue(pre.gamma, juce::dontSendNotification);
        warpModeCombo->combo.setText(warpName(pre.warpMode), juce::dontSendNotification);
        dpWarpAmount->slider.setValue(pre.warpAmount, juce::dontSendNotification);
        dpWarpP->slider.setValue(pre.warpP, juce::dontSendNotification);
        dpWarpCenter->slider.setValue(pre.warpCenter, juce::dontSendNotification);
        dpFocusCenter->slider.setValue(pre.focusCenter, juce::dontSendNotification);
        dpFocusWidth->slider.setValue(pre.focusWidth, juce::dontSendNotification);
        dpFocusAmount->slider.setValue(pre.focusAmount, juce::dontSendNotification);
        dpAmpRandom->slider.setValue(pre.ampRandom, juce::dontSendNotification);
        dpDecaySpread->slider.setValue(pre.decaySpread, juce::dontSendNotification);
        dpReleaseSpread->slider.setValue(pre.releaseSpread, juce::dontSendNotification);
        phaseInitCombo->combo.setText(phaseInitName(pre.phaseInitMode), juce::dontSendNotification);

        auto &m = p.modal;
        mPartialCount->slider.setValue(m.partialCount, juce::dontSendNotification);
        modalShapeCombo->combo.setText(modalShapeName(m.shape), juce::dontSendNotification);
        mStiffness->slider.setValue(m.stiffness, juce::dontSendNotification);
        mDampingZeta->slider.setValue(m.dampingZeta, juce::dontSendNotification);
        mDampingHF->slider.setValue(m.dampingHF, juce::dontSendNotification);
        mPickup->slider.setValue(m.pickupPosition, juce::dontSendNotification);

        auto &q = p.pde;
        pdePartialCount->slider.setValue(q.partialCount, juce::dontSendNotification);
        pdeBodyCombo->combo.setText(pdeBodyName(q.body), juce::dontSendNotification);
        pdeParam1->slider.setValue(q.param1, juce::dontSendNotification);
        pdeParam2->slider.setValue(q.param2, juce::dontSendNotification);
        pdeDamping->slider.setValue(q.dampingZeta, juce::dontSendNotification);
        pdeDampingHF->slider.setValue(q.dampingHF, juce::dontSendNotification);
        pdePickup->slider.setValue(q.pickupPosition, juce::dontSendNotification);

        // saPartialCount slider drives FunctionalSampleSource.partialCount.
        saPartialCount->slider.setValue(p.functionalSource.partialCount, juce::dontSendNotification);

        const auto &un = p.unison;
        unisonVoices->slider.setValue(un.voices, juce::dontSendNotification);
        unisonDetune->slider.setValue(un.detuneCents, juce::dontSendNotification);
        unisonWidth->slider.setValue(un.widthStereo, juce::dontSendNotification);
        unisonPhase->slider.setValue(un.phaseSpread, juce::dontSendNotification);

        partialMaxRefHz->slider.setValue(p.partialMaxRefHz, juce::dontSendNotification);
        refreshPartialMaxLabel(p.partialMaxRefHz);

        visibleType = p.type;
        suspendCallbacks = false;
        updateVisibility();
        resized();
    }

    void refreshPartialMaxLabel(float refHz)
    {
        const int n = synth::maxPartialCountForRefHz(refHz);
        partialMaxLabel.setText("Max N = " + juce::String(n)
                                  + " (cap = " + juce::String(synth::kMaxPartials) + ")",
                                juce::dontSendNotification);
    }

  private:
    void updateVisibility()
    {
        bool d = (visibleType == synth::GeneratorType::DirectPartial);
        bool m = (visibleType == synth::GeneratorType::ModalODE);
        bool q = (visibleType == synth::GeneratorType::PDEModal);
        bool ps = (visibleType == synth::GeneratorType::FunctionalSampleSource);
        bool samp = (visibleType == synth::GeneratorType::SamplePlayback);

        dpPartialCount->setVisible(d);

        for(auto *c : std::vector<juce::Component *> { dpInharmonic.get(),
                          dpTilt.get(), dpLambda.get(), dpGamma.get(), dpFocusCenter.get(),
                          dpFocusWidth.get(), dpFocusAmount.get(), dpWarpAmount.get(),
                          dpWarpP.get(), dpWarpCenter.get(), dpAmpRandom.get(),
                          dpDecaySpread.get(), dpReleaseSpread.get(),
                          freqShapeCombo.get(), warpModeCombo.get(), phaseInitCombo.get() })
            c->setVisible(!samp);
        for(auto *c : std::vector<juce::Component *> { mPartialCount.get(), mStiffness.get(),
                          mDampingZeta.get(), mDampingHF.get(), mPickup.get(),
                          modalShapeCombo.get() })
            c->setVisible(m);
        for(auto *c : std::vector<juce::Component *> { pdePartialCount.get(), pdeParam1.get(),
                          pdeParam2.get(), pdeDamping.get(), pdeDampingHF.get(),
                          pdePickup.get(), pdeBodyCombo.get() })
            c->setVisible(q);
        saPartialCount->setVisible(ps);
        partialSetButton.setVisible(ps);
        partialSetLabel.setVisible(ps);
        for(auto *c : std::vector<juce::Component *> {
                          fssAttackSharp.get(), fssBrightness.get(), fssBody.get(),
                          fssRootHz.get(), fssQualityCombo.get() })
            c->setVisible(ps);
        fssRootLock.setVisible(ps);
        samplePlaybackButton.setVisible(samp);
        samplePlaybackLabel.setVisible(samp);
        sampleLoop.setVisible(samp);
        sampleReverse.setVisible(samp);
        for(auto *c : std::vector<juce::Component *> { sampleRootMidi.get(), sampleStart.get(),
                          sampleEnd.get(), sampleGain.get(), samplePitch.get(), sampleAttack.get(),
                          sampleRelease.get() })
            c->setVisible(samp);

        // Unison + partial-max controls are always visible (shared across generators).
        for(auto *c : std::vector<juce::Component *> { unisonVoices.get(), unisonDetune.get(),
                          unisonWidth.get(), unisonPhase.get(),
                          partialMaxRefHz.get() })
            c->setVisible(true);
        partialMaxButton.setVisible(true);
        partialMaxLabel.setVisible(true);
    }

    void wireCallbacks()
    {
        auto push = [this] {
            if(suspendCallbacks) return;
            synth::SourceGenParams p = core.getGeneratorParams();
            p.type = stringToGenerator(typeCombo.combo.getText());

            p.direct.partialCount = (int)std::lround(dpPartialCount->slider.getValue());
            p.pre.freqShape = stringToFreqShape(freqShapeCombo->combo.getText());
            p.pre.inharmonicAmount = (float)dpInharmonic->slider.getValue();
            p.pre.tilt = (float)dpTilt->slider.getValue();
            p.pre.lambda = (float)dpLambda->slider.getValue();
            p.pre.gamma = (float)dpGamma->slider.getValue();
            p.pre.warpMode = stringToWarp(warpModeCombo->combo.getText());
            p.pre.warpAmount = (float)dpWarpAmount->slider.getValue();
            p.pre.warpP = (float)dpWarpP->slider.getValue();
            p.pre.warpCenter = (float)dpWarpCenter->slider.getValue();
            p.pre.focusCenter = (float)dpFocusCenter->slider.getValue();
            p.pre.focusWidth = (float)dpFocusWidth->slider.getValue();
            p.pre.focusAmount = (float)dpFocusAmount->slider.getValue();
            p.pre.ampRandom = (float)dpAmpRandom->slider.getValue();
            p.pre.decaySpread = (float)dpDecaySpread->slider.getValue();
            p.pre.releaseSpread = (float)dpReleaseSpread->slider.getValue();
            p.pre.phaseInitMode = stringToPhaseInit(phaseInitCombo->combo.getText());

            p.modal.partialCount = (int)std::lround(mPartialCount->slider.getValue());
            p.modal.shape = stringToModalShape(modalShapeCombo->combo.getText());
            p.modal.stiffness = (float)mStiffness->slider.getValue();
            p.modal.dampingZeta = (float)mDampingZeta->slider.getValue();
            p.modal.dampingHF = (float)mDampingHF->slider.getValue();
            p.modal.pickupPosition = (float)mPickup->slider.getValue();

            p.pde.partialCount = (int)std::lround(pdePartialCount->slider.getValue());
            p.pde.body = stringToPDEBody(pdeBodyCombo->combo.getText());
            p.pde.param1 = (float)pdeParam1->slider.getValue();
            p.pde.param2 = (float)pdeParam2->slider.getValue();
            p.pde.dampingZeta = (float)pdeDamping->slider.getValue();
            p.pde.dampingHF = (float)pdeDampingHF->slider.getValue();
            p.pde.pickupPosition = (float)pdePickup->slider.getValue();

            p.functionalSource.partialCount = (int)std::lround(saPartialCount->slider.getValue());
            p.functionalSource.attackSharpness = (float)fssAttackSharp->slider.getValue();
            p.functionalSource.brightnessDecay = (float)fssBrightness->slider.getValue();
            p.functionalSource.bodyResonance   = (float)fssBody->slider.getValue();
            p.functionalSource.userRootHz      = (float)fssRootHz->slider.getValue();
            p.functionalSource.userLockRoot    = fssRootLock.getToggleState();
            {
                const int qid = fssQualityCombo->combo.getSelectedId();
                p.functionalSource.quality =
                    (qid == 1) ? synth::FunctionalSampleQuality::Draft :
                    (qid == 3) ? synth::FunctionalSampleQuality::High :
                                 synth::FunctionalSampleQuality::Standard;
            }
            p.samplePlayback.rootMidi = (float)sampleRootMidi->slider.getValue();
            p.samplePlayback.start01 = (float)sampleStart->slider.getValue();
            p.samplePlayback.end01 = std::max(p.samplePlayback.start01 + 0.01f, (float)sampleEnd->slider.getValue());
            p.samplePlayback.playbackGain = (float)sampleGain->slider.getValue();
            p.samplePlayback.pitchOffsetSemitones = (float)samplePitch->slider.getValue();
            p.samplePlayback.attackMs = (float)sampleAttack->slider.getValue();
            p.samplePlayback.releaseMs = (float)sampleRelease->slider.getValue();
            p.samplePlayback.loopEnabled = sampleLoop.getToggleState();
            p.samplePlayback.reverse = sampleReverse.getToggleState();

            p.unison.voices = (int)std::lround(unisonVoices->slider.getValue());
            p.unison.detuneCents = (float)unisonDetune->slider.getValue();
            p.unison.widthStereo = (float)unisonWidth->slider.getValue();
            p.unison.phaseSpread = (float)unisonPhase->slider.getValue();

            p.partialMaxRefHz = (float)partialMaxRefHz->slider.getValue();
            refreshPartialMaxLabel(p.partialMaxRefHz);

            core.setGeneratorParams(p);
            visibleType = p.type;
            updateVisibility();
            resized();
        };

        typeCombo.combo.onChange = push;
        fssQualityCombo->combo.onChange = push;
        fssRootLock.onClick = push;
        sampleLoop.onClick = push;
        sampleReverse.onClick = push;
        freqShapeCombo->combo.onChange = push;
        warpModeCombo->combo.onChange = push;
        phaseInitCombo->combo.onChange = push;
        modalShapeCombo->combo.onChange = push;
        pdeBodyCombo->combo.onChange = push;

        for(auto *k : std::vector<KnobWithLabel *> {
                 dpPartialCount.get(), dpInharmonic.get(), dpTilt.get(), dpLambda.get(),
                 dpGamma.get(), dpFocusCenter.get(), dpFocusWidth.get(), dpFocusAmount.get(),
                 dpWarpAmount.get(), dpWarpP.get(), dpWarpCenter.get(), dpAmpRandom.get(),
                 dpDecaySpread.get(), dpReleaseSpread.get(),
                 mPartialCount.get(), mStiffness.get(), mDampingZeta.get(), mDampingHF.get(),
                 mPickup.get(),
                 pdePartialCount.get(), pdeParam1.get(), pdeParam2.get(), pdeDamping.get(),
                 pdeDampingHF.get(), pdePickup.get(),
                 saPartialCount.get(),
                 fssAttackSharp.get(), fssBrightness.get(), fssBody.get(), fssRootHz.get(),
                 sampleRootMidi.get(), sampleStart.get(), sampleEnd.get(), sampleGain.get(),
                 samplePitch.get(), sampleAttack.get(), sampleRelease.get(),
                 unisonVoices.get(), unisonDetune.get(), unisonWidth.get(), unisonPhase.get(),
                 partialMaxRefHz.get() })
            k->slider.onValueChange = push;

        partialMaxButton.onClick = [this] {
            if(suspendCallbacks) return;
            const float refHz = (float)partialMaxRefHz->slider.getValue();
            const int n = synth::maxPartialCountForRefHz(refHz);
            // Apply to whichever partial-count slider is currently active.
            const auto t = stringToGenerator(typeCombo.combo.getText());
            switch(t)
            {
                case synth::GeneratorType::DirectPartial:
                    dpPartialCount->slider.setValue(n, juce::sendNotificationSync); break;
                case synth::GeneratorType::ModalODE:
                    mPartialCount->slider.setValue(n, juce::sendNotificationSync); break;
                case synth::GeneratorType::PDEModal:
                    pdePartialCount->slider.setValue(n, juce::sendNotificationSync); break;
                case synth::GeneratorType::FunctionalSampleSource:
                    saPartialCount->slider.setValue(n, juce::sendNotificationSync); break;
                case synth::GeneratorType::SamplePlayback:
                    break;
            }
            refreshPartialMaxLabel(refHz);
        };

        partialSetButton.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>("Open monophonic partial source",
                juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                "*.wav;*.aif;*.aiff;*.flac");
            const auto cfFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
            chooser->launchAsync(cfFlags, [this](const juce::FileChooser &fc) {
                auto file = fc.getResult();
                if(file == juce::File {})
                    return;
                auto p = core.getGeneratorParams();
                p.functionalSource.filePath = file.getFullPathName().toStdString();
                p.type = synth::GeneratorType::FunctionalSampleSource;
                core.setGeneratorParams(p);
                partialSetLabel.setText(file.getFileName(), juce::dontSendNotification);
                syncFromCore();
            });
        };
        samplePlaybackButton.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>("Open playable sample",
                juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                "*.wav;*.aif;*.aiff;*.flac;*.ogg");
            const auto cfFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
            chooser->launchAsync(cfFlags, [this](const juce::FileChooser &fc) {
                auto file = fc.getResult();
                if(file == juce::File {}) return;
                auto p = core.getGeneratorParams();
                p.samplePlayback.filePath = file.getFullPathName().toStdString();
                p.type = synth::GeneratorType::SamplePlayback;
                core.setGeneratorParams(p);
                core.importSamplePlaybackFile(p.samplePlayback.filePath);
                syncFromCore();
            });
        };
    }

    synth::SynthCore &core;
    juce::Viewport viewport;
    juce::Component content;
    SpectrumView spectrumView;
    CompactPerformancePanel performancePanel;
    bool suspendCallbacks = false;
    synth::GeneratorType visibleType = synth::GeneratorType::DirectPartial;

    ComboWithLabel typeCombo { "GeneratorType" };

    std::unique_ptr<KnobWithLabel> dpPartialCount, dpInharmonic, dpTilt, dpLambda, dpGamma;
    std::unique_ptr<KnobWithLabel> dpFocusCenter, dpFocusWidth, dpFocusAmount;
    std::unique_ptr<KnobWithLabel> dpWarpAmount, dpWarpP, dpWarpCenter;
    std::unique_ptr<KnobWithLabel> dpAmpRandom, dpDecaySpread, dpReleaseSpread;
    std::unique_ptr<ComboWithLabel> freqShapeCombo, warpModeCombo, phaseInitCombo;
    juce::TextButton partialSetButton;
    juce::Label partialSetLabel;
    juce::TextButton samplePlaybackButton;
    juce::Label samplePlaybackLabel;
    juce::ToggleButton sampleLoop, sampleReverse;

    std::unique_ptr<KnobWithLabel> mPartialCount, mStiffness, mDampingZeta, mDampingHF, mPickup;
    std::unique_ptr<ComboWithLabel> modalShapeCombo;

    std::unique_ptr<KnobWithLabel> pdePartialCount, pdeParam1, pdeParam2, pdeDamping, pdeDampingHF, pdePickup;
    std::unique_ptr<ComboWithLabel> pdeBodyCombo;

    std::unique_ptr<KnobWithLabel> saPartialCount;
    // FunctionalSampleSource macro / quality / root override controls.
    std::unique_ptr<KnobWithLabel> fssAttackSharp;
    std::unique_ptr<KnobWithLabel> fssBrightness;
    std::unique_ptr<KnobWithLabel> fssBody;
    std::unique_ptr<KnobWithLabel> fssRootHz;
    std::unique_ptr<ComboWithLabel> fssQualityCombo;
    std::unique_ptr<KnobWithLabel> sampleRootMidi, sampleStart, sampleEnd, sampleGain, samplePitch;
    std::unique_ptr<KnobWithLabel> sampleAttack, sampleRelease;
    juce::ToggleButton fssRootLock;

    std::unique_ptr<KnobWithLabel> unisonVoices, unisonDetune, unisonWidth, unisonPhase;
    std::unique_ptr<KnobWithLabel> partialMaxRefHz;
    juce::TextButton partialMaxButton;
    juce::Label partialMaxLabel;

    std::unique_ptr<juce::FileChooser> chooser;
};

// =============================================================================
// Operators panel - 5 fixed slots
// =============================================================================
class OperatorsPanel : public juce::Component
{
  public:
    static constexpr int kSlots = 5;
    OperatorsPanel(synth::SynthCore &c) : core(c)
    {
        for(int i = 0; i < kSlots; ++i)
        {
            auto &row = rows[i];
            row.enable.setButtonText("on");
            addAndMakeVisible(row.enable);
            row.typeCombo.combo.addItem("PartialMask", 1);
            row.typeCombo.combo.addItem("AmpScalePerGroup", 2);
            row.typeCombo.combo.addItem("FrequencyJitter", 3);
            row.typeCombo.combo.addItem("SpectralTilt", 4);
            row.typeCombo.combo.addItem("HarmonicLock", 5);
            row.typeCombo.combo.setSelectedId(1, juce::dontSendNotification);
            row.typeCombo.label.setText("op[" + juce::String(i) + "]", juce::dontSendNotification);
            addAndMakeVisible(row.typeCombo);

            row.k1 = std::make_unique<KnobWithLabel>("p1", -1.0, 1.0, 0.001);
            row.k2 = std::make_unique<KnobWithLabel>("p2", -1.0, 1.0, 0.001);
            row.k3 = std::make_unique<KnobWithLabel>("p3", -1.0, 1.0, 0.001);
            row.k4 = std::make_unique<KnobWithLabel>("p4", -1.0, 1.0, 0.001);
            addAndMakeVisible(*row.k1);
            addAndMakeVisible(*row.k2);
            addAndMakeVisible(*row.k3);
            addAndMakeVisible(*row.k4);
        }
        wireCallbacks();
        syncFromCore();
    }
    void refreshFromCore() { syncFromCore(); }
    void resized() override
    {
        auto a = getLocalBounds().reduced(8);
        const int rowH = 100;
        for(int i = 0; i < kSlots; ++i)
        {
            auto r = a.removeFromTop(rowH);
            rows[i].enable.setBounds(r.removeFromLeft(50).reduced(4, 36));
            rows[i].typeCombo.setBounds(r.removeFromLeft(180).reduced(2));
            int w = r.getWidth() / 4;
            rows[i].k1->setBounds(r.removeFromLeft(w).reduced(2));
            rows[i].k2->setBounds(r.removeFromLeft(w).reduced(2));
            rows[i].k3->setBounds(r.removeFromLeft(w).reduced(2));
            rows[i].k4->setBounds(r.reduced(2));
        }
    }

  private:
    struct Row
    {
        juce::ToggleButton enable;
        ComboWithLabel typeCombo { "type" };
        std::unique_ptr<KnobWithLabel> k1, k2, k3, k4;
    };
    void syncFromCore()
    {
        suspend = true;
        auto chain = core.getOperatorChain();
        chain.ops.resize(kSlots);
        for(int i = 0; i < kSlots; ++i)
        {
            auto &op = chain.ops[i];
            auto &r = rows[i];
            r.enable.setToggleState(op.enabled, juce::dontSendNotification);
            r.typeCombo.combo.setSelectedId((int)op.type + 1, juce::dontSendNotification);
            updateRowKnobLabels(r, op);
            applyOpToKnobs(op, r);
        }
        suspend = false;
    }
    void updateRowKnobLabels(Row &r, const synth::OperatorBase &op)
    {
        switch(op.type)
        {
            case synth::OperatorType::PartialMask:
                r.k1->label.setText("maskLow", juce::dontSendNotification); r.k1->slider.setRange(0, synth::kMaxPartials, 1);
                r.k2->label.setText("maskHigh", juce::dontSendNotification); r.k2->slider.setRange(0, synth::kMaxPartials, 1);
                r.k3->label.setText("groupLow|Mid|High bits", juce::dontSendNotification); r.k3->slider.setRange(0, 7, 1);
                r.k4->label.setText("(unused)", juce::dontSendNotification); r.k4->slider.setRange(0, 1, 1);
                break;
            case synth::OperatorType::AmpScalePerGroup:
                r.k1->label.setText("gainLow (mu=0)", juce::dontSendNotification); r.k1->slider.setRange(0, 4, 0.001);
                r.k2->label.setText("gainMid (mu=1)", juce::dontSendNotification); r.k2->slider.setRange(0, 4, 0.001);
                r.k3->label.setText("gainHigh (mu=2)", juce::dontSendNotification); r.k3->slider.setRange(0, 4, 0.001);
                r.k4->label.setText("(unused)", juce::dontSendNotification); r.k4->slider.setRange(0, 1, 1);
                break;
            case synth::OperatorType::FrequencyJitter:
                r.k1->label.setText("jitterAmount", juce::dontSendNotification); r.k1->slider.setRange(0, 1, 0.001);
                r.k2->label.setText("seed", juce::dontSendNotification); r.k2->slider.setRange(1, 65535, 1);
                r.k3->label.setText("(unused)", juce::dontSendNotification); r.k3->slider.setRange(0, 1, 1);
                r.k4->label.setText("(unused)", juce::dontSendNotification); r.k4->slider.setRange(0, 1, 1);
                break;
            case synth::OperatorType::SpectralTilt:
                r.k1->label.setText("extraTilt", juce::dontSendNotification); r.k1->slider.setRange(-2, 2, 0.001);
                r.k2->label.setText("(unused)", juce::dontSendNotification); r.k2->slider.setRange(0, 1, 1);
                r.k3->label.setText("(unused)", juce::dontSendNotification); r.k3->slider.setRange(0, 1, 1);
                r.k4->label.setText("(unused)", juce::dontSendNotification); r.k4->slider.setRange(0, 1, 1);
                break;
            case synth::OperatorType::HarmonicLock:
                r.k1->label.setText("lockAmount", juce::dontSendNotification); r.k1->slider.setRange(0, 1, 0.001);
                r.k2->label.setText("(unused)", juce::dontSendNotification); r.k2->slider.setRange(0, 1, 1);
                r.k3->label.setText("(unused)", juce::dontSendNotification); r.k3->slider.setRange(0, 1, 1);
                r.k4->label.setText("(unused)", juce::dontSendNotification); r.k4->slider.setRange(0, 1, 1);
                break;
        }
    }
    void applyOpToKnobs(const synth::OperatorBase &op, Row &r)
    {
        switch(op.type)
        {
            case synth::OperatorType::PartialMask:
            {
                r.k1->slider.setValue(op.maskLow, juce::dontSendNotification);
                r.k2->slider.setValue(op.maskHigh, juce::dontSendNotification);
                int bits = (op.maskGroupLow ? 1 : 0) | (op.maskGroupMid ? 2 : 0) | (op.maskGroupHigh ? 4 : 0);
                r.k3->slider.setValue(bits, juce::dontSendNotification);
                break;
            }
            case synth::OperatorType::AmpScalePerGroup:
                r.k1->slider.setValue(op.gainLow, juce::dontSendNotification);
                r.k2->slider.setValue(op.gainMid, juce::dontSendNotification);
                r.k3->slider.setValue(op.gainHigh, juce::dontSendNotification);
                break;
            case synth::OperatorType::FrequencyJitter:
                r.k1->slider.setValue(op.jitterAmount, juce::dontSendNotification);
                r.k2->slider.setValue((double)op.jitterSeed, juce::dontSendNotification);
                break;
            case synth::OperatorType::SpectralTilt:
                r.k1->slider.setValue(op.extraTilt, juce::dontSendNotification);
                break;
            case synth::OperatorType::HarmonicLock:
                r.k1->slider.setValue(op.lockAmount, juce::dontSendNotification);
                break;
        }
    }
    synth::OperatorBase rowToOp(int slot)
    {
        auto &r = rows[slot];
        synth::OperatorBase op;
        op.enabled = r.enable.getToggleState();
        op.type = (synth::OperatorType)(r.typeCombo.combo.getSelectedId() - 1);
        switch(op.type)
        {
            case synth::OperatorType::PartialMask:
            {
                op.maskLow = (int)std::lround(r.k1->slider.getValue());
                op.maskHigh = (int)std::lround(r.k2->slider.getValue());
                int bits = (int)std::lround(r.k3->slider.getValue());
                op.maskGroupLow = (bits & 1) != 0;
                op.maskGroupMid = (bits & 2) != 0;
                op.maskGroupHigh = (bits & 4) != 0;
                break;
            }
            case synth::OperatorType::AmpScalePerGroup:
                op.gainLow = (float)r.k1->slider.getValue();
                op.gainMid = (float)r.k2->slider.getValue();
                op.gainHigh = (float)r.k3->slider.getValue();
                break;
            case synth::OperatorType::FrequencyJitter:
                op.jitterAmount = (float)r.k1->slider.getValue();
                op.jitterSeed = (uint32_t)std::lround(r.k2->slider.getValue());
                break;
            case synth::OperatorType::SpectralTilt:
                op.extraTilt = (float)r.k1->slider.getValue();
                break;
            case synth::OperatorType::HarmonicLock:
                op.lockAmount = (float)r.k1->slider.getValue();
                break;
        }
        return op;
    }
    void wireCallbacks()
    {
        auto push = [this] {
            if(suspend) return;
            synth::OperatorChain chain;
            chain.ops.resize(kSlots);
            for(int i = 0; i < kSlots; ++i)
                chain.ops[i] = rowToOp(i);
            core.setOperatorChain(chain);
        };
        for(int i = 0; i < kSlots; ++i)
        {
            auto &r = rows[i];
            r.enable.onClick = push;
            r.typeCombo.combo.onChange = [this, i, push] {
                if(suspend) return;
                synth::OperatorBase op = rowToOp(i);
                op.type = (synth::OperatorType)(rows[i].typeCombo.combo.getSelectedId() - 1);
                updateRowKnobLabels(rows[i], op);
                applyOpToKnobs(op, rows[i]);
                push();
            };
            r.k1->slider.onValueChange = push;
            r.k2->slider.onValueChange = push;
            r.k3->slider.onValueChange = push;
            r.k4->slider.onValueChange = push;
        }
    }

    synth::SynthCore &core;
    std::array<Row, kSlots> rows;
    bool suspend = false;
};

// =============================================================================
// Matrix panel: 8 LFOs + 16 routing rules
// =============================================================================
class MatrixPanel : public juce::Component
{
  public:
    MatrixPanel(synth::SynthCore &c) : core(c)
    {
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&content, false);
        viewport.setScrollBarsShown(true, false);

        chaosEnable.setButtonText("chaos");
        content.addAndMakeVisible(chaosEnable);
        chaosTitle.setText("Chaos Source", juce::dontSendNotification);
        chaosTitle.setColour(juce::Label::textColourId, juce::Colours::white);
        content.addAndMakeVisible(chaosTitle);
        chaosType.label.setText("noise", juce::dontSendNotification);
        chaosType.combo.addItem("White", 1);
        chaosType.combo.addItem("Smooth", 2);
        chaosType.combo.addItem("Crackle", 3);
        chaosType.combo.setSelectedId(2, juce::dontSendNotification);
        content.addAndMakeVisible(chaosType);
        chaosRate = std::make_unique<KnobWithLabel>("chaos Hz", 0.01, 80.0, 0.01);
        chaosAmount = std::make_unique<KnobWithLabel>("chaos amt", 0.0, 1.0, 0.001);
        content.addAndMakeVisible(*chaosRate);
        content.addAndMakeVisible(*chaosAmount);

        shapeTitle.setText("Shape Source", juce::dontSendNotification);
        shapeTitle.setColour(juce::Label::textColourId, juce::Colours::white);
        content.addAndMakeVisible(shapeTitle);
        shapeAxis.setButtonText("use x_i");
        content.addAndMakeVisible(shapeAxis);
        shapeType.label.setText("shape", juce::dontSendNotification);
        shapeType.combo.addItem("Asymm", 1);
        shapeType.combo.addItem("Sine", 2);
        shapeType.combo.addItem("Square", 3);
        shapeType.combo.addItem("Tri", 4);
        shapeType.combo.addItem("S/H", 5);
        shapeType.combo.setSelectedId(1, juce::dontSendNotification);
        content.addAndMakeVisible(shapeType);
        shapePhase = std::make_unique<KnobWithLabel>("shape phase", 0.0, 1.0, 0.001);
        shapeRho = std::make_unique<KnobWithLabel>("shape rho", 0.01, 0.99, 0.001);
        shapePUp = std::make_unique<KnobWithLabel>("shape p_up", 0.1, 5.0, 0.001);
        shapePDown = std::make_unique<KnobWithLabel>("shape p_down", 0.1, 5.0, 0.001);
        content.addAndMakeVisible(*shapePhase);
        content.addAndMakeVisible(*shapeRho);
        content.addAndMakeVisible(*shapePUp);
        content.addAndMakeVisible(*shapePDown);

        for(int i = 0; i < synth::kMaxLfos; ++i)
        {
            auto &lr = lfoRows[i];
            lr.enable.setButtonText("on");
            content.addAndMakeVisible(lr.enable);
            lr.title.setText("LFO" + juce::String(i + 1), juce::dontSendNotification);
            lr.title.setColour(juce::Label::textColourId, juce::Colours::white);
            content.addAndMakeVisible(lr.title);
            lr.shape.combo.addItem("Asymm", 1);
            lr.shape.combo.addItem("Sine", 2);
            lr.shape.combo.addItem("Square", 3);
            lr.shape.combo.addItem("Tri", 4);
            lr.shape.combo.addItem("S/H", 5);
            lr.shape.combo.setSelectedId(1, juce::dontSendNotification);
            lr.shape.label.setText("shape", juce::dontSendNotification);
            content.addAndMakeVisible(lr.shape);
            lr.freq = std::make_unique<KnobWithLabel>("f_LFO Hz", 0.01, 30.0, 0.01);
            lr.rho = std::make_unique<KnobWithLabel>("rho_lfo", 0.01, 0.99, 0.001);
            lr.pUp = std::make_unique<KnobWithLabel>("p_u", 0.1, 5.0, 0.001);
            lr.pDown = std::make_unique<KnobWithLabel>("p_d", 0.1, 5.0, 0.001);
            lr.curve = std::make_unique<LfoCurveView>(core, i);
            content.addAndMakeVisible(*lr.freq);
            content.addAndMakeVisible(*lr.rho);
            content.addAndMakeVisible(*lr.pUp);
            content.addAndMakeVisible(*lr.pDown);
            content.addAndMakeVisible(*lr.curve);
        }
        for(int i = 0; i < synth::kMaxMatrixRules; ++i)
        {
            auto &rr = ruleRows[i];
            rr.enable.setButtonText("on");
            content.addAndMakeVisible(rr.enable);
            rr.title.setText("R" + juce::String(i + 1), juce::dontSendNotification);
            rr.title.setColour(juce::Label::textColourId, juce::Colours::white);
            content.addAndMakeVisible(rr.title);
            rr.source.combo.addItem("-", 1);
            for(int j = 1; j <= 8; ++j)
                rr.source.combo.addItem("LFO" + juce::String(j), 1 + j);
            rr.source.combo.addItem("Velocity", 10);
            rr.source.combo.addItem("KeyTrack", 11);
            rr.source.combo.addItem("Random", 12);
            rr.source.combo.addItem("ADSR", 13);
            rr.source.combo.addItem("GeneratorSelf", 14);
            rr.source.combo.addItem("Chaos", 15);
            rr.source.combo.addItem("Shape", 16);
            rr.source.label.setText("source m_k", juce::dontSendNotification);
            content.addAndMakeVisible(rr.source);
            rr.dest.combo.addItem("Amp (M_amp)", 1);
            rr.dest.combo.addItem("Freq (M_freq)", 2);
            rr.dest.combo.addItem("Phase (d_phi)", 3);
            rr.dest.combo.addItem("Decay (T_D)", 4);
            rr.dest.label.setText("dest d_k", juce::dontSendNotification);
            content.addAndMakeVisible(rr.dest);
            rr.weight.combo.addItem("All", 1);
            rr.weight.combo.addItem("Low (1-x)", 2);
            rr.weight.combo.addItem("High (x)", 3);
            rr.weight.combo.addItem("mu=0", 4);
            rr.weight.combo.addItem("mu=1", 5);
            rr.weight.combo.addItem("mu=2", 6);
            rr.weight.combo.addItem("Band", 7);
            rr.weight.label.setText("W_k", juce::dontSendNotification);
            content.addAndMakeVisible(rr.weight);
            rr.depth = std::make_unique<KnobWithLabel>("alpha_k depth", -2.0, 2.0, 0.001);
            content.addAndMakeVisible(*rr.depth);
        }
        wireCallbacks();
        syncFromCore();
    }
    void refreshFromCore() { syncFromCore(); }

    void resized() override
    {
        viewport.setBounds(getLocalBounds());
        const int contentW = std::max(900, viewport.getWidth() - 18);
        const int chaosH = 156;
        const int lfoRowH = 190;
        const int rulesTop = chaosH + 2 * lfoRowH + 34;
        const int ruleRowH = 240;
        const int ruleCols = 2;
        const int ruleRowCount = (synth::kMaxMatrixRules + ruleCols - 1) / ruleCols;
        const int contentH = rulesTop + ruleRowCount * ruleRowH + 30;
        content.setSize(contentW, contentH);

        auto a = content.getLocalBounds().reduced(10);
        auto chaosArea = a.removeFromTop(chaosH).reduced(8);
        auto chaosTop = chaosArea.removeFromTop(28);
        chaosTitle.setBounds(chaosTop.removeFromLeft(130));
        chaosEnable.setBounds(chaosTop.removeFromLeft(80));
        chaosType.setBounds(chaosTop.removeFromLeft(180).reduced(4, 0));
        auto chaosKnobs = chaosArea.removeFromTop(46).reduced(0, 4);
        chaosRate->setBounds(chaosKnobs.removeFromLeft(130).reduced(4));
        chaosAmount->setBounds(chaosKnobs.removeFromLeft(130).reduced(4));

        auto shapeRow = chaosArea.removeFromTop(76).reduced(0, 4);
        auto shapeTop = shapeRow.removeFromTop(28);
        shapeTitle.setBounds(shapeTop.removeFromLeft(130));
        shapeAxis.setBounds(shapeTop.removeFromLeft(82));
        shapeType.setBounds(shapeTop.removeFromLeft(180).reduced(4, 0));
        const int skw = std::max(80, shapeRow.getWidth() / 4);
        shapePhase->setBounds(shapeRow.removeFromLeft(skw).reduced(4));
        shapeRho->setBounds(shapeRow.removeFromLeft(skw).reduced(4));
        shapePUp->setBounds(shapeRow.removeFromLeft(skw).reduced(4));
        shapePDown->setBounds(shapeRow.removeFromLeft(skw).reduced(4));

        auto lfoArea = a.removeFromTop(2 * lfoRowH + 16);
        const int colW = lfoArea.getWidth() / 4;
        for(int i = 0; i < synth::kMaxLfos; ++i)
        {
            const int col = i % 4;
            const int row = i / 4;
            auto cell = juce::Rectangle<int>(lfoArea.getX() + col * colW,
                                             lfoArea.getY() + row * lfoRowH,
                                             colW, lfoRowH).reduced(8);
            auto top = cell.removeFromTop(24);
            lfoRows[i].title.setBounds(top.removeFromLeft(50));
            lfoRows[i].enable.setBounds(top.removeFromLeft(44));
            lfoRows[i].shape.setBounds(top);
            lfoRows[i].curve->setBounds(cell.removeFromTop(54).reduced(2, 4));
            int kw = cell.getWidth() / 4;
            auto knobs = cell.removeFromTop(104);
            lfoRows[i].freq->setBounds(knobs.removeFromLeft(kw).reduced(3));
            lfoRows[i].rho->setBounds(knobs.removeFromLeft(kw).reduced(3));
            lfoRows[i].pUp->setBounds(knobs.removeFromLeft(kw).reduced(3));
            lfoRows[i].pDown->setBounds(knobs.reduced(3));
        }

        auto rulesArea = a.reduced(0, 6);
        const int rcolW = rulesArea.getWidth() / ruleCols;
        for(int i = 0; i < synth::kMaxMatrixRules; ++i)
        {
            const int col = i % ruleCols;
            const int row = i / ruleCols;
            auto cell = juce::Rectangle<int>(rulesArea.getX() + col * rcolW,
                                             rulesArea.getY() + row * ruleRowH,
                                             rcolW, ruleRowH).reduced(10);

            auto top = cell.removeFromTop(26);
            ruleRows[i].title.setBounds(top.removeFromLeft(40));
            ruleRows[i].enable.setBounds(top.removeFromLeft(52));

            auto comboBlock = cell.removeFromTop(102);
            auto srcLine = comboBlock.removeFromTop(50);
            ruleRows[i].source.setBounds(srcLine.removeFromLeft(srcLine.getWidth() / 2).reduced(4));
            ruleRows[i].dest.setBounds(srcLine.reduced(4));

            auto wLine = comboBlock.removeFromTop(50);
            ruleRows[i].weight.setBounds(wLine.removeFromLeft(wLine.getWidth() / 2).reduced(4));

            auto depthArea = cell.reduced(4, 0);
            ruleRows[i].depth->setBounds(depthArea);
        }
    }

  private:
    struct LfoRow
    {
        juce::Label title;
        juce::ToggleButton enable;
        ComboWithLabel shape { "shape" };
        std::unique_ptr<KnobWithLabel> freq, rho, pUp, pDown;
        std::unique_ptr<LfoCurveView> curve;
    };
    struct RuleRow
    {
        juce::Label title;
        juce::ToggleButton enable;
        ComboWithLabel source { "source" };
        ComboWithLabel dest { "dest" };
        ComboWithLabel weight { "W" };
        std::unique_ptr<KnobWithLabel> depth;
    };

    void syncFromCore()
    {
        suspend = true;
        auto chaos = core.getChaosParams();
        chaosEnable.setToggleState(chaos.enabled, juce::dontSendNotification);
        chaosType.combo.setSelectedId((int)chaos.type + 1, juce::dontSendNotification);
        chaosRate->slider.setValue(chaos.frequencyHz, juce::dontSendNotification);
        chaosAmount->slider.setValue(chaos.amount, juce::dontSendNotification);
        auto shape = core.getShapeSourceParams();
        shapeAxis.setToggleState(shape.useSpectralX, juce::dontSendNotification);
        shapeType.combo.setSelectedId((int)shape.shape + 1, juce::dontSendNotification);
        shapePhase->slider.setValue(shape.phase0, juce::dontSendNotification);
        shapeRho->slider.setValue(shape.rho, juce::dontSendNotification);
        shapePUp->slider.setValue(shape.pUp, juce::dontSendNotification);
        shapePDown->slider.setValue(shape.pDown, juce::dontSendNotification);
        for(int i = 0; i < synth::kMaxLfos; ++i)
        {
            auto p = core.getLfoParams(i);
            auto &lr = lfoRows[i];
            lr.enable.setToggleState(p.enabled, juce::dontSendNotification);
            lr.shape.combo.setSelectedId((int)p.shape + 1, juce::dontSendNotification);
            lr.freq->slider.setValue(p.frequencyHz, juce::dontSendNotification);
            lr.rho->slider.setValue(p.rhoLfo, juce::dontSendNotification);
            lr.pUp->slider.setValue(p.pUp, juce::dontSendNotification);
            lr.pDown->slider.setValue(p.pDown, juce::dontSendNotification);
        }
        for(int i = 0; i < synth::kMaxMatrixRules; ++i)
        {
            auto r = core.getMatrixRule(i);
            auto &rr = ruleRows[i];
            rr.enable.setToggleState(r.enabled, juce::dontSendNotification);
            rr.source.combo.setSelectedId((int)r.source + 1, juce::dontSendNotification);
            rr.dest.combo.setSelectedId((int)r.dest + 1, juce::dontSendNotification);
            rr.weight.combo.setSelectedId((int)r.weight + 1, juce::dontSendNotification);
            rr.depth->slider.setValue(r.depth, juce::dontSendNotification);
        }
        suspend = false;
    }
    void wireCallbacks()
    {
        auto pushChaos = [this] {
            if(suspend) return;
            synth::ChaosParams p;
            p.enabled = chaosEnable.getToggleState();
            p.type = (synth::ChaosNoiseType)(chaosType.combo.getSelectedId() - 1);
            p.frequencyHz = (float)chaosRate->slider.getValue();
            p.amount = (float)chaosAmount->slider.getValue();
            core.setChaosParams(p);
        };
        chaosEnable.onClick = pushChaos;
        chaosType.combo.onChange = pushChaos;
        chaosRate->slider.onValueChange = pushChaos;
        chaosAmount->slider.onValueChange = pushChaos;

        auto pushShape = [this] {
            if(suspend) return;
            synth::ShapeSourceParams p;
            p.useSpectralX = shapeAxis.getToggleState();
            p.shape = (synth::LfoShape)(shapeType.combo.getSelectedId() - 1);
            p.phase0 = (float)shapePhase->slider.getValue();
            p.rho = (float)shapeRho->slider.getValue();
            p.pUp = (float)shapePUp->slider.getValue();
            p.pDown = (float)shapePDown->slider.getValue();
            core.setShapeSourceParams(p);
        };
        shapeAxis.onClick = pushShape;
        shapeType.combo.onChange = pushShape;
        shapePhase->slider.onValueChange = pushShape;
        shapeRho->slider.onValueChange = pushShape;
        shapePUp->slider.onValueChange = pushShape;
        shapePDown->slider.onValueChange = pushShape;

        for(int i = 0; i < synth::kMaxLfos; ++i)
        {
            auto pushLfo = [this, i] {
                if(suspend) return;
                synth::LfoParams p;
                p.enabled = lfoRows[i].enable.getToggleState();
                p.shape = (synth::LfoShape)(lfoRows[i].shape.combo.getSelectedId() - 1);
                p.frequencyHz = (float)lfoRows[i].freq->slider.getValue();
                p.rhoLfo = (float)lfoRows[i].rho->slider.getValue();
                p.pUp = (float)lfoRows[i].pUp->slider.getValue();
                p.pDown = (float)lfoRows[i].pDown->slider.getValue();
                core.setLfoParams(i, p);
            };
            lfoRows[i].enable.onClick = pushLfo;
            lfoRows[i].shape.combo.onChange = pushLfo;
            lfoRows[i].freq->slider.onValueChange = pushLfo;
            lfoRows[i].rho->slider.onValueChange = pushLfo;
            lfoRows[i].pUp->slider.onValueChange = pushLfo;
            lfoRows[i].pDown->slider.onValueChange = pushLfo;
        }
        for(int i = 0; i < synth::kMaxMatrixRules; ++i)
        {
            auto pushRule = [this, i] {
                if(suspend) return;
                synth::MatrixRule r;
                r.enabled = ruleRows[i].enable.getToggleState();
                r.source = (synth::ModSource)(ruleRows[i].source.combo.getSelectedId() - 1);
                r.dest = (synth::ModDestination)(ruleRows[i].dest.combo.getSelectedId() - 1);
                r.weight = (synth::WeightMode)(ruleRows[i].weight.combo.getSelectedId() - 1);
                r.depth = (float)ruleRows[i].depth->slider.getValue();
                core.setMatrixRule(i, r);
            };
            ruleRows[i].enable.onClick = pushRule;
            ruleRows[i].source.combo.onChange = pushRule;
            ruleRows[i].dest.combo.onChange = pushRule;
            ruleRows[i].weight.combo.onChange = pushRule;
            ruleRows[i].depth->slider.onValueChange = pushRule;
        }
    }

    synth::SynthCore &core;
    juce::Viewport viewport;
    juce::Component content;
    juce::Label chaosTitle;
    juce::ToggleButton chaosEnable;
    ComboWithLabel chaosType { "noise" };
    std::unique_ptr<KnobWithLabel> chaosRate, chaosAmount;
    juce::Label shapeTitle;
    juce::ToggleButton shapeAxis;
    ComboWithLabel shapeType { "shape" };
    std::unique_ptr<KnobWithLabel> shapePhase, shapeRho, shapePUp, shapePDown;
    std::array<LfoRow, synth::kMaxLfos> lfoRows;
    std::array<RuleRow, synth::kMaxMatrixRules> ruleRows;
    bool suspend = false;
};

// =============================================================================
// Performance panel - global ADSR + curves + global gain + voice meter
// =============================================================================
class PerformancePanel : public juce::Component, private juce::Timer
{
  public:
    PerformancePanel(synth::SynthCore &c) : core(c), adsrCurve(c)
    {
        addAndMakeVisible(adsrCurve);

        attack = std::make_unique<KnobWithLabel>("T_A attack(s)", 0.001, 4.0, 0.001);
        decay = std::make_unique<KnobWithLabel>("T_D decay(s)", 0.005, 4.0, 0.001);
        sustain = std::make_unique<KnobWithLabel>("S sustain", 0.0, 1.0, 0.001);
        release = std::make_unique<KnobWithLabel>("T_R release(s)", 0.005, 8.0, 0.001);
        etaA = std::make_unique<KnobWithLabel>("eta_A", 0.5, 16.0, 0.01);
        etaD = std::make_unique<KnobWithLabel>("eta_D", 0.5, 16.0, 0.01);
        etaR = std::make_unique<KnobWithLabel>("eta_R", 0.5, 16.0, 0.01);
        gain = std::make_unique<KnobWithLabel>("globalGain", 0.0, 1.0, 0.001);
        addAndMakeVisible(*attack); addAndMakeVisible(*decay);
        addAndMakeVisible(*sustain); addAndMakeVisible(*release);
        addAndMakeVisible(*etaA); addAndMakeVisible(*etaD); addAndMakeVisible(*etaR);
        addAndMakeVisible(*gain);

        modeA = std::make_unique<ComboWithLabel>("modeA");
        modeD = std::make_unique<ComboWithLabel>("modeD");
        modeR = std::make_unique<ComboWithLabel>("modeR");
        for(auto *cw : {modeA.get(), modeD.get(), modeR.get()})
        {
            cw->combo.addItem("Exp", 1);
            cw->combo.addItem("Power", 2);
            cw->combo.addItem("Sigmoid", 3);
            addAndMakeVisible(*cw);
        }
        voiceLabel.setText("Voices: 0/16", juce::dontSendNotification);
        voiceLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        addAndMakeVisible(voiceLabel);

        wireCallbacks();
        syncFromCore();
        startTimerHz(15);
    }
    void refreshFromCore() { syncFromCore(); }
    void resized() override
    {
        auto a = getLocalBounds().reduced(10);
        adsrCurve.setBounds(a.removeFromTop(170).reduced(4));

        auto r1 = a.removeFromTop(110);
        int w = r1.getWidth() / 4;
        attack->setBounds(r1.removeFromLeft(w).reduced(4));
        decay->setBounds(r1.removeFromLeft(w).reduced(4));
        sustain->setBounds(r1.removeFromLeft(w).reduced(4));
        release->setBounds(r1.reduced(4));

        auto r2 = a.removeFromTop(110);
        int w2 = r2.getWidth() / 6;
        modeA->setBounds(r2.removeFromLeft(w2).reduced(4));
        etaA->setBounds(r2.removeFromLeft(w2).reduced(4));
        modeD->setBounds(r2.removeFromLeft(w2).reduced(4));
        etaD->setBounds(r2.removeFromLeft(w2).reduced(4));
        modeR->setBounds(r2.removeFromLeft(w2).reduced(4));
        etaR->setBounds(r2.reduced(4));

        auto r3 = a.removeFromTop(110);
        gain->setBounds(r3.removeFromLeft(w).reduced(4));
        voiceLabel.setBounds(r3.reduced(8));
    }

  private:
    void timerCallback() override
    {
        voiceLabel.setText("Voices: " + juce::String(core.getActiveVoiceCount()) + "/" + juce::String(synth::kMaxVoices),
                           juce::dontSendNotification);
    }
    void syncFromCore()
    {
        suspend = true;
        auto a = core.getGlobalAdsr();
        attack->slider.setValue(a.attack, juce::dontSendNotification);
        decay->slider.setValue(a.decay, juce::dontSendNotification);
        sustain->slider.setValue(a.sustain, juce::dontSendNotification);
        release->slider.setValue(a.release, juce::dontSendNotification);
        etaA->slider.setValue(a.etaA, juce::dontSendNotification);
        etaD->slider.setValue(a.etaD, juce::dontSendNotification);
        etaR->slider.setValue(a.etaR, juce::dontSendNotification);
        modeA->combo.setSelectedId((int)a.attackCurve + 1, juce::dontSendNotification);
        modeD->combo.setSelectedId((int)a.decayCurve + 1, juce::dontSendNotification);
        modeR->combo.setSelectedId((int)a.releaseCurve + 1, juce::dontSendNotification);
        gain->slider.setValue(core.getGlobalGain(), juce::dontSendNotification);
        suspend = false;
    }
    void wireCallbacks()
    {
        auto pushAdsr = [this] {
            if(suspend) return;
            synth::GlobalAdsrParams a;
            a.attack = (float)attack->slider.getValue();
            a.decay = (float)decay->slider.getValue();
            a.sustain = (float)sustain->slider.getValue();
            a.release = (float)release->slider.getValue();
            a.etaA = (float)etaA->slider.getValue();
            a.etaD = (float)etaD->slider.getValue();
            a.etaR = (float)etaR->slider.getValue();
            a.attackCurve = (synth::EnvCurve)(modeA->combo.getSelectedId() - 1);
            a.decayCurve = (synth::EnvCurve)(modeD->combo.getSelectedId() - 1);
            a.releaseCurve = (synth::EnvCurve)(modeR->combo.getSelectedId() - 1);
            core.setGlobalAdsr(a);
        };
        for(auto *k : {attack.get(), decay.get(), sustain.get(), release.get(),
                        etaA.get(), etaD.get(), etaR.get()})
            k->slider.onValueChange = pushAdsr;
        for(auto *cw : {modeA.get(), modeD.get(), modeR.get()})
            cw->combo.onChange = pushAdsr;
        gain->slider.onValueChange = [this] {
            if(suspend) return;
            core.setGlobalGain((float)gain->slider.getValue());
        };
    }

    synth::SynthCore &core;
    AdsrCurveView adsrCurve;
    std::unique_ptr<KnobWithLabel> attack, decay, sustain, release;
    std::unique_ptr<KnobWithLabel> etaA, etaD, etaR;
    std::unique_ptr<KnobWithLabel> gain;
    std::unique_ptr<ComboWithLabel> modeA, modeD, modeR;
    juce::Label voiceLabel;
    bool suspend = false;
};

// =============================================================================
// Final effects panel: post-voice EQ + filter chain
// =============================================================================
class EffectsPanel : public juce::Component
{
  public:
    EffectsPanel(synth::SynthCore &c, bool postResample = false)
        : core(c), usePostResample(postResample)
    {
        eqTitle.setText(usePostResample ? "Post Resample EQ" : "EQ", juce::dontSendNotification);
        filterTitle.setText(usePostResample ? "Post Resample Filter" : "Filter", juce::dontSendNotification);
        for(auto *l : { &eqTitle, &filterTitle })
        {
            l->setColour(juce::Label::textColourId, juce::Colours::white);
            l->setFont(juce::Font(juce::FontOptions(18.0f)));
            addAndMakeVisible(*l);
        }

        eqEnable.setButtonText("on");
        filterEnable.setButtonText("on");
        addAndMakeVisible(eqEnable);
        addAndMakeVisible(filterEnable);

        eqMode.label.setText("mode", juce::dontSendNotification);
        filterMode.label.setText("mode", juce::dontSendNotification);
        for(auto *cbox : { &eqMode, &filterMode })
        {
            cbox->combo.addItem("Normal", 1);
            cbox->combo.addItem("Linear", 2);
            cbox->combo.addItem("Nonlinear", 3);
            cbox->combo.setSelectedId(1, juce::dontSendNotification);
            addAndMakeVisible(*cbox);
        }
        filterType.label.setText("type", juce::dontSendNotification);
        filterType.combo.addItem("LowPass", 1);
        filterType.combo.addItem("HighPass", 2);
        filterType.combo.addItem("BandPass", 3);
        filterType.combo.setSelectedId(1, juce::dontSendNotification);
        addAndMakeVisible(filterType);

        eqLow = std::make_unique<KnobWithLabel>("low dB", -18.0, 18.0, 0.1);
        eqMid = std::make_unique<KnobWithLabel>("mid dB", -18.0, 18.0, 0.1);
        eqHigh = std::make_unique<KnobWithLabel>("high dB", -18.0, 18.0, 0.1);
        eqDrive = std::make_unique<KnobWithLabel>("eq drive", 0.1, 8.0, 0.01);
        filterCutoff = std::make_unique<KnobWithLabel>("cutoff Hz", 20.0, 20000.0, 1.0);
        filterRes = std::make_unique<KnobWithLabel>("resonance", 0.0, 0.95, 0.001);
        filterDrive = std::make_unique<KnobWithLabel>("filter drive", 0.1, 8.0, 0.01);
        for(auto *k : { eqLow.get(), eqMid.get(), eqHigh.get(), eqDrive.get(),
                        filterCutoff.get(), filterRes.get(), filterDrive.get() })
            addAndMakeVisible(*k);

        wireCallbacks();
        syncFromCore();
    }

    void refreshFromCore() { syncFromCore(); }

    void resized() override
    {
        auto a = getLocalBounds().reduced(18);
        auto top = a.removeFromTop(a.getHeight() / 2).reduced(0, 8);
        auto bottom = a.reduced(0, 8);

        auto eqHeader = top.removeFromTop(36);
        eqTitle.setBounds(eqHeader.removeFromLeft(80));
        eqEnable.setBounds(eqHeader.removeFromLeft(70));
        eqMode.setBounds(eqHeader.removeFromLeft(170).reduced(4, 0));
        int eqW = std::max(90, top.getWidth() / 4);
        eqLow->setBounds(top.removeFromLeft(eqW).reduced(6));
        eqMid->setBounds(top.removeFromLeft(eqW).reduced(6));
        eqHigh->setBounds(top.removeFromLeft(eqW).reduced(6));
        eqDrive->setBounds(top.removeFromLeft(eqW).reduced(6));

        auto filterHeader = bottom.removeFromTop(36);
        filterTitle.setBounds(filterHeader.removeFromLeft(100));
        filterEnable.setBounds(filterHeader.removeFromLeft(70));
        filterMode.setBounds(filterHeader.removeFromLeft(170).reduced(4, 0));
        filterType.setBounds(filterHeader.removeFromLeft(170).reduced(4, 0));
        int fw = std::max(100, bottom.getWidth() / 3);
        filterCutoff->setBounds(bottom.removeFromLeft(fw).reduced(6));
        filterRes->setBounds(bottom.removeFromLeft(fw).reduced(6));
        filterDrive->setBounds(bottom.removeFromLeft(fw).reduced(6));
    }

  private:
    void syncFromCore()
    {
        suspend = true;
        auto p = usePostResample ? core.getPostResampleEffectsParams()
                                 : core.getEffectsParams();
        eqEnable.setToggleState(p.eq.enabled, juce::dontSendNotification);
        eqMode.combo.setSelectedId((int)p.eq.mode + 1, juce::dontSendNotification);
        eqLow->slider.setValue(p.eq.lowGainDb, juce::dontSendNotification);
        eqMid->slider.setValue(p.eq.midGainDb, juce::dontSendNotification);
        eqHigh->slider.setValue(p.eq.highGainDb, juce::dontSendNotification);
        eqDrive->slider.setValue(p.eq.drive, juce::dontSendNotification);
        filterEnable.setToggleState(p.filter.enabled, juce::dontSendNotification);
        filterMode.combo.setSelectedId((int)p.filter.mode + 1, juce::dontSendNotification);
        filterType.combo.setSelectedId((int)p.filter.type + 1, juce::dontSendNotification);
        filterCutoff->slider.setValue(p.filter.cutoffHz, juce::dontSendNotification);
        filterRes->slider.setValue(p.filter.resonance, juce::dontSendNotification);
        filterDrive->slider.setValue(p.filter.drive, juce::dontSendNotification);
        suspend = false;
    }

    void wireCallbacks()
    {
        auto push = [this] {
            if(suspend) return;
            synth::EffectsChainParams p;
            p.eq.enabled = eqEnable.getToggleState();
            p.eq.mode = (synth::EffectProcessMode)(eqMode.combo.getSelectedId() - 1);
            p.eq.lowGainDb = (float)eqLow->slider.getValue();
            p.eq.midGainDb = (float)eqMid->slider.getValue();
            p.eq.highGainDb = (float)eqHigh->slider.getValue();
            p.eq.drive = (float)eqDrive->slider.getValue();
            p.filter.enabled = filterEnable.getToggleState();
            p.filter.mode = (synth::EffectProcessMode)(filterMode.combo.getSelectedId() - 1);
            p.filter.type = (synth::FilterType)(filterType.combo.getSelectedId() - 1);
            p.filter.cutoffHz = (float)filterCutoff->slider.getValue();
            p.filter.resonance = (float)filterRes->slider.getValue();
            p.filter.drive = (float)filterDrive->slider.getValue();
            if(usePostResample)
                core.setPostResampleEffectsParams(p);
            else
                core.setEffectsParams(p);
        };
        eqEnable.onClick = push;
        filterEnable.onClick = push;
        eqMode.combo.onChange = push;
        filterMode.combo.onChange = push;
        filterType.combo.onChange = push;
        for(auto *k : { eqLow.get(), eqMid.get(), eqHigh.get(), eqDrive.get(),
                        filterCutoff.get(), filterRes.get(), filterDrive.get() })
            k->slider.onValueChange = push;
    }

    synth::SynthCore &core;
    bool usePostResample = false;
    juce::Label eqTitle, filterTitle;
    juce::ToggleButton eqEnable, filterEnable;
    ComboWithLabel eqMode { "mode" }, filterMode { "mode" }, filterType { "type" };
    std::unique_ptr<KnobWithLabel> eqLow, eqMid, eqHigh, eqDrive;
    std::unique_ptr<KnobWithLabel> filterCutoff, filterRes, filterDrive;
    bool suspend = false;
};

// =============================================================================
// Resampling engine panel: recorder + frozen buffer processor
// =============================================================================
class ResamplingWaveformView : public juce::Component, private juce::Timer
{
  public:
    ResamplingWaveformView(synth::SynthCore &c) : core(c) { startTimerHz(20); }
    void paint(juce::Graphics &g) override
    {
        const auto state = core.getResamplingDisplayState();
        auto area = getLocalBounds().reduced(6);
        g.fillAll(juce::Colour(0xff121518));
        g.setColour(juce::Colour(0xff2f353b));
        g.drawRoundedRectangle(area.toFloat(), 4.0f, 1.0f);
        auto top = area.removeFromTop(20);
        g.setColour(juce::Colours::lightgrey);
        g.setFont(12.0f);
        const juce::String transport = state.recording ? "REC" : (state.playing ? "PLAY" : "STOP");
        g.drawText(transport + "    "
                   + juce::String(state.currentSeconds, 2) + " s / "
                   + juce::String(state.durationSeconds, 2) + " s",
                   top, juce::Justification::centredLeft);
        auto wave = area.reduced(2, 6);
        g.setColour(juce::Colour(0xff273038));
        g.drawHorizontalLine(wave.getCentreY(), (float)wave.getX(), (float)wave.getRight());
        juce::Path p;
        for(size_t i = 0; i < state.waveform.size(); ++i)
        {
            const float x = wave.getX() + float(i) * float(wave.getWidth()) / float(state.waveform.size() - 1);
            const float y = wave.getCentreY() - state.waveform[i] * 0.46f * float(wave.getHeight());
            if(i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        g.setColour(juce::Colours::aqua);
        g.strokePath(p, juce::PathStrokeType(1.6f));
        g.setColour(juce::Colours::orange);
        const float px = wave.getX() + state.playhead01 * float(wave.getWidth());
        g.drawVerticalLine((int)px, (float)wave.getY(), (float)wave.getBottom());
        if(state.recording)
        {
            g.setColour(juce::Colours::red);
            const float rx = wave.getX() + state.recordhead01 * float(wave.getWidth());
            g.drawVerticalLine((int)rx, (float)wave.getY(), (float)wave.getBottom());
        }
    }
  private:
    void timerCallback() override { repaint(); }
    synth::SynthCore &core;
};

class ResamplingPanel : public juce::Component
{
  public:
    ResamplingPanel(synth::SynthCore &c) : core(c), waveform(c)
    {
        enable.setButtonText("Engine");
        record.setButtonText("Record");
        playButton.setButtonText("Play");
        pauseButton.setButtonText("Pause");
        loop.setButtonText("Loop");
        reverse.setButtonText("Reverse");
        importButton.setButtonText("Import Audio...");
        clearButton.setButtonText("Clear Buffer");
        status.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        addAndMakeVisible(enable);
        addAndMakeVisible(record);
        addAndMakeVisible(playButton);
        addAndMakeVisible(pauseButton);
        addAndMakeVisible(loop);
        addAndMakeVisible(reverse);
        addAndMakeVisible(importButton);
        addAndMakeVisible(clearButton);
        addAndMakeVisible(status);
        addAndMakeVisible(waveform);

        auto add = [this](std::unique_ptr<KnobWithLabel> &slot, const char *name,
                          double mn, double mx, double st) {
            slot = std::make_unique<KnobWithLabel>(name, mn, mx, st);
            addAndMakeVisible(*slot);
        };
        add(recordSeconds, "record s", 0.25, 60.0, 0.25);
        add(dryWet, "dry wet", 0.0, 1.0, 0.001);
        add(playbackGain, "play gain", 0.0, 2.0, 0.001);
        add(pitch, "pitch semi", -24.0, 24.0, 0.01);
        add(sliceStart, "slice start", 0.0, 0.99, 0.001);
        add(sliceEnd, "slice end", 0.01, 1.0, 0.001);
        add(sliceCount, "slice count", 1.0, 32.0, 1.0);
        add(sliceRotate, "slice rotate", -31.0, 31.0, 1.0);
        add(granular, "granular", 0.0, 1.0, 0.001);
        add(grainSize, "grain ms", 5.0, 400.0, 1.0);
        add(stutter, "stutter", 0.0, 1.0, 0.001);
        add(stutterRate, "stutter Hz", 0.25, 32.0, 0.01);

        wireCallbacks();
        syncFromCore();
    }

    void refreshFromCore() { syncFromCore(); }

    void resized() override
    {
        auto a = getLocalBounds().reduced(16);
        auto header = a.removeFromTop(42);
        for(auto *b : { &enable, &record, &loop, &reverse })
            b->setBounds(header.removeFromLeft(110).reduced(3));
        playButton.setBounds(header.removeFromLeft(86).reduced(3));
        pauseButton.setBounds(header.removeFromLeft(86).reduced(3));
        importButton.setBounds(header.removeFromLeft(150).reduced(3));
        clearButton.setBounds(header.removeFromLeft(130).reduced(3));
        status.setBounds(header.reduced(6, 3));

        waveform.setBounds(a.removeFromTop(150).reduced(4));
        auto row1 = a.removeFromTop(a.getHeight() / 2);
        auto row2 = a;
        const int w1 = std::max(84, row1.getWidth() / 6);
        for(auto *k : { recordSeconds.get(), dryWet.get(), playbackGain.get(), pitch.get(),
                        sliceStart.get(), sliceEnd.get() })
            k->setBounds(row1.removeFromLeft(w1).reduced(4));
        const int w2 = std::max(84, row2.getWidth() / 6);
        for(auto *k : { sliceCount.get(), sliceRotate.get(), granular.get(), grainSize.get(),
                        stutter.get(), stutterRate.get() })
            k->setBounds(row2.removeFromLeft(w2).reduced(4));
    }

  private:
    void syncFromCore()
    {
        suspend = true;
        const auto p = core.getResamplingParams();
        enable.setToggleState(p.enabled, juce::dontSendNotification);
        record.setToggleState(p.recording, juce::dontSendNotification);
        loop.setToggleState(p.loopEnabled, juce::dontSendNotification);
        reverse.setToggleState(p.reverse, juce::dontSendNotification);
        recordSeconds->slider.setValue(p.recordSeconds, juce::dontSendNotification);
        dryWet->slider.setValue(p.dryWet, juce::dontSendNotification);
        playbackGain->slider.setValue(p.playbackGain, juce::dontSendNotification);
        pitch->slider.setValue(p.pitchSemitones, juce::dontSendNotification);
        sliceStart->slider.setValue(p.sliceStart, juce::dontSendNotification);
        sliceEnd->slider.setValue(p.sliceEnd, juce::dontSendNotification);
        sliceCount->slider.setValue(p.sliceCount, juce::dontSendNotification);
        sliceRotate->slider.setValue(p.sliceRotate, juce::dontSendNotification);
        granular->slider.setValue(p.granularAmount, juce::dontSendNotification);
        grainSize->slider.setValue(p.grainSizeMs, juce::dontSendNotification);
        stutter->slider.setValue(p.stutterAmount, juce::dontSendNotification);
        stutterRate->slider.setValue(p.stutterRateHz, juce::dontSendNotification);
        status.setText(core.hasResampleBuffer() ? "buffer ready" : "buffer empty",
                       juce::dontSendNotification);
        suspend = false;
    }

    void wireCallbacks()
    {
        auto push = [this] {
            if(suspend) return;
            synth::ResamplingEngineParams p;
            const auto old = core.getResamplingParams();
            p.enabled = enable.getToggleState();
            p.recording = record.getToggleState();
            p.playbackEnabled = old.playbackEnabled;
            p.loopEnabled = loop.getToggleState();
            p.reverse = reverse.getToggleState();
            p.recordSeconds = (float)recordSeconds->slider.getValue();
            p.dryWet = (float)dryWet->slider.getValue();
            p.playbackGain = (float)playbackGain->slider.getValue();
            p.pitchSemitones = (float)pitch->slider.getValue();
            p.sliceStart = (float)sliceStart->slider.getValue();
            p.sliceEnd = std::max(p.sliceStart + 0.01f, (float)sliceEnd->slider.getValue());
            p.sliceCount = (int)std::lround(sliceCount->slider.getValue());
            p.sliceRotate = (int)std::lround(sliceRotate->slider.getValue());
            p.granularAmount = (float)granular->slider.getValue();
            p.grainSizeMs = (float)grainSize->slider.getValue();
            p.stutterAmount = (float)stutter->slider.getValue();
            p.stutterRateHz = (float)stutterRate->slider.getValue();
            p.importedFilePath = old.importedFilePath;
            core.setResamplingParams(p);
            syncFromCore();
        };
        for(auto *b : { &enable, &record, &loop, &reverse })
            b->onClick = push;
        playButton.onClick = [this, push] {
            auto p = core.getResamplingParams();
            p.enabled = true;
            p.playbackEnabled = true;
            core.setResamplingParams(p);
            syncFromCore();
        };
        pauseButton.onClick = [this] {
            auto p = core.getResamplingParams();
            p.playbackEnabled = false;
            core.setResamplingParams(p);
            syncFromCore();
        };
        for(auto *k : { recordSeconds.get(), dryWet.get(), playbackGain.get(), pitch.get(),
                        sliceStart.get(), sliceEnd.get(), sliceCount.get(), sliceRotate.get(),
                        granular.get(), grainSize.get(), stutter.get(), stutterRate.get() })
            k->slider.onValueChange = push;

        importButton.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>("Import resample buffer",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                "*.wav;*.aif;*.aiff;*.flac;*.ogg");
            chooser->launchAsync(juce::FileBrowserComponent::openMode
                                 | juce::FileBrowserComponent::canSelectFiles,
                                 [this](const juce::FileChooser &fc) {
                const auto file = fc.getResult();
                if(file == juce::File {}) return;
                const bool ok = core.importResampleBuffer(file.getFullPathName().toStdString());
                status.setText(ok ? "imported: " + file.getFileName() : "import failed",
                               juce::dontSendNotification);
                syncFromCore();
            });
        };
        clearButton.onClick = [this] {
            core.clearResampleBuffer();
            syncFromCore();
        };
    }

    synth::SynthCore &core;
    juce::ToggleButton enable, record, loop, reverse;
    juce::TextButton playButton, pauseButton;
    juce::TextButton importButton, clearButton;
    juce::Label status;
    ResamplingWaveformView waveform;
    std::unique_ptr<KnobWithLabel> recordSeconds, dryWet, playbackGain, pitch;
    std::unique_ptr<KnobWithLabel> sliceStart, sliceEnd, sliceCount, sliceRotate;
    std::unique_ptr<KnobWithLabel> granular, grainSize, stutter, stutterRate;
    std::unique_ptr<juce::FileChooser> chooser;
    bool suspend = false;
};

// =============================================================================
// Piano Locks editor v1: note blocks + per-note lock inspector
// =============================================================================
class PianoLocksPanel : public juce::Component
{
  public:
    PianoLocksPanel(synth::SynthCore &c) : core(c)
    {
        addDemoButton.setButtonText("Seed Demo Notes");
        addLockButton.setButtonText("Add Pitch Lock");
        gainLockButton.setButtonText("Add Gain Lock");
        title.setText("Piano Locks", juce::dontSendNotification);
        title.setColour(juce::Label::textColourId, juce::Colours::white);
        selectedLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        noteLabel.setText("Note", juce::dontSendNotification);
        velocityLabel.setText("Velocity", juce::dontSendNotification);
        pitchLabel.setText("Pitch Lock", juce::dontSendNotification);
        gainLabel.setText("Gain Lock", juce::dontSendNotification);
        for(auto *l : { &noteLabel, &velocityLabel, &pitchLabel, &gainLabel })
        {
            l->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible(*l);
        }
        velocity.setRange(0.0, 1.0, 0.001);
        velocity.setSliderStyle(juce::Slider::LinearHorizontal);
        velocity.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        pitchLock.setRange(-24.0, 24.0, 0.01);
        pitchLock.setSliderStyle(juce::Slider::LinearHorizontal);
        pitchLock.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        gainLock.setRange(0.0, 2.0, 0.001);
        gainLock.setSliderStyle(juce::Slider::LinearHorizontal);
        gainLock.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        std::array<juce::Component *, 8> inspectorComponents {
            &title, &selectedLabel, &addDemoButton, &addLockButton,
            &gainLockButton, &velocity, &pitchLock, &gainLock
        };
        for(auto *cpt : inspectorComponents)
            addAndMakeVisible(*cpt);

        addDemoButton.onClick = [this] { seedDemoNotes(); };
        addLockButton.onClick = [this] { setOrCreatePitchLock(); };
        gainLockButton.onClick = [this] { setOrCreateGainLock(); };
        velocity.onValueChange = [this] { writeInspectorToProject(false, false); };
        pitchLock.onValueChange = [this] { writeInspectorToProject(true, false); };
        gainLock.onValueChange = [this] { writeInspectorToProject(false, true); };
        syncFromCore();
    }

    void refreshFromCore() { syncFromCore(); }

    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colour(0xff121518));
        auto area = getLocalBounds().reduced(16);
        auto editor = area.removeFromLeft((int)(area.getWidth() * 0.68f)).reduced(4);
        g.setColour(juce::Colour(0xff20262b));
        g.fillRoundedRectangle(editor.toFloat(), 4.0f);
        g.setColour(juce::Colour(0xff38424a));
        for(int r = 0; r <= 24; ++r)
        {
            const float y = editor.getY() + float(r) * float(editor.getHeight()) / 24.0f;
            g.drawHorizontalLine((int)y, (float)editor.getX(), (float)editor.getRight());
        }
        for(int b = 0; b <= 16; ++b)
        {
            const float x = editor.getX() + float(b) * float(editor.getWidth()) / 16.0f;
            g.drawVerticalLine((int)x, (float)editor.getY(), (float)editor.getBottom());
        }
        if(auto *motif = currentMotif())
        {
            for(const auto &note : motif->noteEvents)
            {
                const auto rect = noteRect(editor, note);
                const bool selected = note.id == selectedNoteId;
                g.setColour(selected ? juce::Colours::orange : juce::Colours::cornflowerblue);
                g.fillRoundedRectangle(rect.toFloat(), 3.0f);
                if(!note.parameterLocks.empty())
                {
                    g.setColour(juce::Colours::yellow);
                    g.fillEllipse(float(rect.getRight() - 8), float(rect.getY() + 3), 5.0f, 5.0f);
                }
            }
        }
    }

    void mouseDown(const juce::MouseEvent &e) override
    {
        auto area = getLocalBounds().reduced(16);
        auto editor = area.removeFromLeft((int)(area.getWidth() * 0.68f)).reduced(4);
        if(auto *motif = currentMotif())
        {
            for(const auto &note : motif->noteEvents)
            {
                if(noteRect(editor, note).contains(e.getPosition()))
                {
                    selectedNoteId = note.id;
                    syncInspector();
                    repaint();
                    return;
                }
            }
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(16);
        auto inspector = area.removeFromRight((int)(area.getWidth() * 0.31f)).reduced(8);
        title.setBounds(inspector.removeFromTop(28));
        selectedLabel.setBounds(inspector.removeFromTop(24));
        addDemoButton.setBounds(inspector.removeFromTop(30).reduced(0, 2));
        inspector.removeFromTop(8);
        noteLabel.setBounds(inspector.removeFromTop(18));
        inspector.removeFromTop(4);
        velocityLabel.setBounds(inspector.removeFromTop(18));
        velocity.setBounds(inspector.removeFromTop(30));
        pitchLabel.setBounds(inspector.removeFromTop(18));
        pitchLock.setBounds(inspector.removeFromTop(30));
        addLockButton.setBounds(inspector.removeFromTop(30).reduced(0, 2));
        gainLabel.setBounds(inspector.removeFromTop(18));
        gainLock.setBounds(inspector.removeFromTop(30));
        gainLockButton.setBounds(inspector.removeFromTop(30).reduced(0, 2));
    }

  private:
    static juce::Rectangle<int> noteRect(const juce::Rectangle<int> &editor,
                                         const synth::PianoNoteEvent &note)
    {
        const float x = editor.getX() + (note.startBeats / 16.0f) * float(editor.getWidth());
        const float w = std::max(14.0f, (note.durationBeats / 16.0f) * float(editor.getWidth()));
        const float yNorm = juce::jlimit(0.0f, 1.0f, (84.0f - float(note.midiNote)) / 24.0f);
        const float y = editor.getY() + yNorm * float(editor.getHeight());
        const float h = std::max(12.0f, float(editor.getHeight()) / 24.0f - 2.0f);
        return { (int)x, (int)y, (int)w, (int)h };
    }

    synth::MotifDefinition *currentMotif()
    {
        if(project.motifs.empty()) return nullptr;
        return &project.motifs.front();
    }

    synth::PianoNoteEvent *selectedNote()
    {
        auto *motif = currentMotif();
        if(motif == nullptr) return nullptr;
        for(auto &note : motif->noteEvents)
            if(note.id == selectedNoteId)
                return &note;
        return nullptr;
    }

    void syncFromCore()
    {
        project = core.getCompositionProject();
        if(project.motifs.empty())
        {
            synth::MotifDefinition motif;
            motif.id = 1;
            motif.name = "Motif A";
            project.motifs.push_back(motif);
            core.setCompositionProject(project);
        }
        if(auto *motif = currentMotif())
        {
            if(selectedNoteId == 0 && !motif->noteEvents.empty())
                selectedNoteId = motif->noteEvents.front().id;
        }
        syncInspector();
        repaint();
    }

    void seedDemoNotes()
    {
        if(auto *motif = currentMotif())
        {
            motif->noteEvents.clear();
            motif->noteEvents.push_back({ 101, 60, 0.0f, 1.0f, 0.80f, "current", {} });
            motif->noteEvents.push_back({ 102, 64, 1.5f, 1.0f, 0.72f, "current", {} });
            motif->noteEvents.push_back({ 103, 67, 3.0f, 1.5f, 0.86f, "current", {} });
            selectedNoteId = 101;
            core.setCompositionProject(project);
            syncInspector();
            repaint();
        }
    }

    void syncInspector()
    {
        suspend = true;
        if(auto *note = selectedNote())
        {
            selectedLabel.setText("Selected note #" + juce::String((juce::int64)note->id),
                                  juce::dontSendNotification);
            noteLabel.setText("MIDI " + juce::String(note->midiNote)
                                  + "  start " + juce::String(note->startBeats, 2)
                                  + "  dur " + juce::String(note->durationBeats, 2),
                              juce::dontSendNotification);
            velocity.setValue(note->velocity, juce::dontSendNotification);
            pitchLock.setValue(findFloatLock(*note, "note.pitchOffsetSemitones", 0.0f),
                               juce::dontSendNotification);
            gainLock.setValue(findFloatLock(*note, "note.gain", 1.0f),
                              juce::dontSendNotification);
        }
        else
        {
            selectedLabel.setText("No note selected", juce::dontSendNotification);
            noteLabel.setText("Note", juce::dontSendNotification);
            velocity.setValue(0.0, juce::dontSendNotification);
            pitchLock.setValue(0.0, juce::dontSendNotification);
            gainLock.setValue(1.0, juce::dontSendNotification);
        }
        suspend = false;
    }

    static float findFloatLock(const synth::PianoNoteEvent &note,
                               const std::string &id,
                               float fallback)
    {
        for(const auto &lock : note.parameterLocks)
            if(lock.target.parameterId == id)
                return lock.value.floatValue;
        return fallback;
    }

    void setOrCreatePitchLock()
    {
        writeInspectorToProject(true, false);
    }

    void setOrCreateGainLock()
    {
        writeInspectorToProject(false, true);
    }

    void writeInspectorToProject(bool pitchChanged, bool gainChanged)
    {
        if(suspend) return;
        auto *note = selectedNote();
        if(note == nullptr) return;
        note->velocity = (float)velocity.getValue();
        if(pitchChanged) setFloatLock(*note, synth::ParameterTargetDomain::GeneratorParam,
                                      "note.pitchOffsetSemitones", (float)pitchLock.getValue());
        if(gainChanged) setFloatLock(*note, synth::ParameterTargetDomain::GeneratorParam,
                                     "note.gain", (float)gainLock.getValue());
        core.setCompositionProject(project);
        repaint();
    }

    static void setFloatLock(synth::PianoNoteEvent &note,
                             synth::ParameterTargetDomain domain,
                             const std::string &id,
                             float value)
    {
        for(auto &lock : note.parameterLocks)
        {
            if(lock.target.parameterId == id)
            {
                lock.value.floatValue = value;
                return;
            }
        }
        synth::ParameterLock lock;
        lock.scope = synth::ParameterScope::Note;
        lock.target.domain = domain;
        lock.target.parameterId = id;
        lock.value.type = synth::LockValueType::Float;
        lock.value.mode = synth::LockValueMode::Override;
        lock.value.floatValue = value;
        note.parameterLocks.push_back(std::move(lock));
    }

    synth::SynthCore &core;
    synth::CompositionProject project;
    uint64_t selectedNoteId = 0;
    bool suspend = false;
    juce::Label title, selectedLabel, noteLabel, velocityLabel, pitchLabel, gainLabel;
    juce::TextButton addDemoButton, addLockButton, gainLockButton;
    juce::Slider velocity, pitchLock, gainLock;
};

// =============================================================================
// Main component
// =============================================================================
class MainComponent : public juce::AudioAppComponent,
                      private juce::MidiKeyboardState::Listener,
                      private juce::MidiInputCallback,
                      private juce::ChangeListener
{
  public:
    MainComponent()
        : keyboardComponent(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
          tabs(juce::TabbedButtonBar::TabsAtTop),
          generatorPanel(synthCore),
          operatorsPanel(synthCore),
          matrixPanel(synthCore),
          effectsPanel(synthCore, false),
          resamplingPanel(synthCore),
          postEffectsPanel(synthCore, true),
          pianoLocksPanel(synthCore)
    {
        setOpaque(true);
        setSize(1400, 880);

        const auto bgColour = juce::Colour(0xff1a1d20);
        tabs.addTab("Seed", bgColour, &generatorPanel, false);
        tabs.addTab("Seed Operators", bgColour, &operatorsPanel, false);
        tabs.addTab("Seed Matrix", bgColour, &matrixPanel, false);
        tabs.addTab("Tone FX", bgColour, &effectsPanel, false);
        tabs.addTab("Creator SampleCraft", bgColour, &resamplingPanel, false);
        tabs.addTab("Mix FX", bgColour, &postEffectsPanel, false);
        tabs.addTab("Creator Piano Locks", bgColour, &pianoLocksPanel, false);
        addAndMakeVisible(tabs);

        addAndMakeVisible(keyboardComponent);
        keyboardState.addListener(this);

        statusLabel.setText("Ready. 16 voices. Play a key.", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        addAndMakeVisible(statusLabel);

        generalLabel.setText("General", juce::dontSendNotification);
        generalLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        generalLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(generalLabel);

        savePresetButton.setButtonText("Export");
        loadPresetButton.setButtonText("Import");
        midiDeviceButton.setButtonText("MIDI");
        audioSettingsButton.setButtonText("Audio");
        panicButton.setButtonText("Panic");
        bufferSizeLabel.setText("Buffer", juce::dontSendNotification);
        bufferSizeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc8d0d8));
        bufferSizeLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(savePresetButton);
        addAndMakeVisible(loadPresetButton);
        addAndMakeVisible(midiDeviceButton);
        addAndMakeVisible(audioSettingsButton);
        addAndMakeVisible(bufferSizeLabel);
        addAndMakeVisible(bufferSizeCombo);
        addAndMakeVisible(panicButton);

        savePresetButton.onClick = [this] { savePreset(); };
        loadPresetButton.onClick = [this] { loadPreset(); };
        midiDeviceButton.onClick = [this] { showMidiDeviceDialog(); };
        audioSettingsButton.onClick = [this] { showAudioSettingsDialog(); };
        bufferSizeCombo.onChange = [this] { applySelectedBufferSize(); };
        panicButton.onClick = [this] { synthCore.allNotesOff(); };

        setAudioChannels(0, 2);
        deviceManager.addChangeListener(this);
        refreshBufferSizeCombo();

        for(const auto &dev : juce::MidiInput::getAvailableDevices())
        {
            deviceManager.setMidiInputDeviceEnabled(dev.identifier, true);
            deviceManager.addMidiInputDeviceCallback(dev.identifier, this);
        }
    }

    ~MainComponent() override
    {
        for(const auto &dev : juce::MidiInput::getAvailableDevices())
            deviceManager.removeMidiInputDeviceCallback(dev.identifier, this);
        deviceManager.removeChangeListener(this);
        keyboardState.removeListener(this);
        shutdownAudio();
    }

    void prepareToPlay(int, double sr) override { synthCore.prepare(sr); }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();
        if(bufferToFill.buffer->getNumChannels() < 1)
            return;
        auto *l = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
        auto *r = bufferToFill.buffer->getNumChannels() > 1
                      ? bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample)
                      : l;
        synthCore.renderBlock(l, r, bufferToFill.numSamples);
    }

    void releaseResources() override {}

    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colour(0xff101214));
        g.setColour(juce::Colours::white);
        g.setFont(20.0f);
        g.drawText("MotifForge v0.3  -  Seed -> Creator -> Motif -> Structure -> Mixing Arrange",
                   getLocalBounds().removeFromTop(28),
                   juce::Justification::centred);

        auto general = getGeneralBoxBounds();
        g.setColour(juce::Colour(0xff171b1f));
        g.fillRoundedRectangle(general.toFloat(), 5.0f);
        g.setColour(juce::Colour(0xff38414a));
        g.drawRoundedRectangle(general.toFloat(), 5.0f, 1.0f);
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced(8);
        a.removeFromTop(28);
        auto systemRow = a.removeFromTop(74);
        auto general = getGeneralBoxBounds();
        auto gbox = general.reduced(8);
        generalLabel.setBounds(gbox.removeFromTop(20));
        auto controlRow = gbox.reduced(0, 4);
        loadPresetButton.setBounds(controlRow.removeFromLeft(78).reduced(2));
        savePresetButton.setBounds(controlRow.removeFromLeft(78).reduced(2));
        midiDeviceButton.setBounds(controlRow.removeFromLeft(68).reduced(2));
        audioSettingsButton.setBounds(controlRow.removeFromLeft(74).reduced(2));
        bufferSizeLabel.setBounds(controlRow.removeFromLeft(54).reduced(2));
        bufferSizeCombo.setBounds(controlRow.removeFromLeft(92).reduced(2));
        panicButton.setBounds(controlRow.removeFromLeft(70).reduced(2));

        auto statusArea = systemRow.withTrimmedLeft(general.getRight() + 10 - systemRow.getX());
        statusLabel.setBounds(statusArea.removeFromBottom(28).reduced(4));

        auto kb = a.removeFromBottom(96);
        keyboardComponent.setBounds(kb.reduced(2));

        tabs.setBounds(a);
    }

  private:
    void handleNoteOn(juce::MidiKeyboardState *, int, int n, float v) override
    {
        synthCore.noteOn(n, v);
    }
    void handleNoteOff(juce::MidiKeyboardState *, int, int n, float) override
    {
        synthCore.noteOff(n);
    }
    void handleIncomingMidiMessage(juce::MidiInput *, const juce::MidiMessage &m) override
    {
        keyboardState.processNextMidiEvent(m);
        if(m.isNoteOn())
            synthCore.noteOn(m.getNoteNumber(), m.getFloatVelocity());
        else if(m.isNoteOff())
            synthCore.noteOff(m.getNoteNumber());
        else if(m.isAllNotesOff() || m.isAllSoundOff())
            synthCore.allNotesOff();
    }
    void changeListenerCallback(juce::ChangeBroadcaster *) override
    {
        refreshBufferSizeCombo();
    }

    juce::Rectangle<int> getGeneralBoxBounds() const
    {
        auto b = getLocalBounds().reduced(8);
        b.removeFromTop(28);
        return b.removeFromTop(66).removeFromLeft(604);
    }

    static juce::var objectProperty(juce::DynamicObject *o,
                                    const juce::Identifier &name,
                                    const juce::var &fallback)
    {
        auto v = o->getProperty(name);
        return v.isVoid() ? fallback : v;
    }

    static juce::var frameToVar(const synth::StaticSpectralFrame &frame)
    {
        auto *o = new juce::DynamicObject();
        o->setProperty("partialCount", frame.partialCount);
        o->setProperty("freqMode", frame.freqMode == synth::FreqMode::RelativeRatio ? "RelativeRatio" : "AbsoluteHz");
        o->setProperty("phaseInit", phaseInitName(frame.phaseInitMode));
        o->setProperty("phaseSeed", (int)frame.phaseSeed);

        juce::Array<juce::var> partials;
        for(int i = 0; i < frame.partialCount; ++i)
        {
            auto *p = new juce::DynamicObject();
            p->setProperty("nu", frame.nu[i]);
            p->setProperty("amp", frame.amp[i]);
            p->setProperty("x", frame.x[i]);
            p->setProperty("mu", (int)frame.mu[i]);
            p->setProperty("phaseLocked", frame.phaseLocked[i]);
            p->setProperty("phaseDriftHz", frame.phaseDriftHz[i]);
            p->setProperty("phaseJitter", frame.phaseJitter[i]);
            p->setProperty("attackScale", frame.attackScale[i]);
            p->setProperty("decayScale", frame.decayScale[i]);
            p->setProperty("sustainLevel", frame.sustainLevel[i]);
            p->setProperty("releaseScale", frame.releaseScale[i]);
            partials.add(juce::var(p));
        }
        o->setProperty("partials", partials);
        return juce::var(o);
    }

    static bool varToFrame(const juce::var &v, synth::StaticSpectralFrame &frame)
    {
        auto *o = v.getDynamicObject();
        if(o == nullptr)
            return false;
        synth::initFrameDefaults(frame);
        frame.partialCount = juce::jlimit(1, synth::kMaxPartials, (int)objectProperty(o, "partialCount", 32));
        frame.freqMode = objectProperty(o, "freqMode", "RelativeRatio").toString() == "AbsoluteHz"
                             ? synth::FreqMode::AbsoluteHz
                             : synth::FreqMode::RelativeRatio;
        frame.phaseInitMode = stringToPhaseInit(objectProperty(o, "phaseInit", "Zero").toString());
        frame.phaseSeed = (uint32_t)(int)objectProperty(o, "phaseSeed", 1);
        if(auto arr = objectProperty(o, "partials", juce::var()); arr.isArray())
        {
            const int n = std::min(frame.partialCount, arr.getArray()->size());
            for(int i = 0; i < n; ++i)
            {
                if(auto *p = arr.getArray()->getReference(i).getDynamicObject())
                {
                    frame.nu[i] = (float)(double)objectProperty(p, "nu", 0.0);
                    frame.amp[i] = (float)(double)objectProperty(p, "amp", 0.0);
                    frame.x[i] = (float)(double)objectProperty(p, "x", 0.0);
                    frame.mu[i] = (uint8_t)(int)objectProperty(p, "mu", 0);
                    frame.phaseLocked[i] = (float)(double)objectProperty(p, "phaseLocked", 0.0);
                    frame.phaseDriftHz[i] = (float)(double)objectProperty(p, "phaseDriftHz", 0.0);
                    frame.phaseJitter[i] = (float)(double)objectProperty(p, "phaseJitter", 0.0);
                    frame.attackScale[i] = (float)(double)objectProperty(p, "attackScale", 1.0);
                    frame.decayScale[i] = (float)(double)objectProperty(p, "decayScale", 1.0);
                    frame.sustainLevel[i] = (float)(double)objectProperty(p, "sustainLevel", 1.0);
                    frame.releaseScale[i] = (float)(double)objectProperty(p, "releaseScale", 1.0);
                }
            }
        }
        return true;
    }

    static juce::var timelineToVar(const synth::SpectralTimeline &timeline)
    {
        auto *o = new juce::DynamicObject();
        o->setProperty("frameCount", timeline.frameCount);
        o->setProperty("durationSeconds", timeline.durationSeconds);
        o->setProperty("loop", timeline.loop);
        juce::Array<juce::var> frames;
        const int n = juce::jlimit(1, synth::kMaxTimelineFrames, timeline.frameCount);
        for(int i = 0; i < n; ++i)
        {
            auto *f = new juce::DynamicObject();
            f->setProperty("time", timeline.timeSeconds[(size_t)i]);
            f->setProperty("frame", frameToVar(timeline.frames[(size_t)i]));
            frames.add(juce::var(f));
        }
        o->setProperty("frames", frames);
        return juce::var(o);
    }

    static bool varToTimeline(const juce::var &v, synth::SpectralTimeline &timeline)
    {
        auto *o = v.getDynamicObject();
        if(o == nullptr)
            return false;
        synth::initTimelineDefaults(timeline);
        timeline.durationSeconds = (float)(double)objectProperty(o, "durationSeconds", 0.0);
        timeline.loop = (bool)objectProperty(o, "loop", false);
        if(auto arr = objectProperty(o, "frames", juce::var()); arr.isArray())
        {
            const int n = std::min(synth::kMaxTimelineFrames, arr.getArray()->size());
            timeline.frameCount = std::max(1, n);
            for(int i = 0; i < n; ++i)
            {
                if(auto *fo = arr.getArray()->getReference(i).getDynamicObject())
                {
                    timeline.timeSeconds[(size_t)i] = (float)(double)objectProperty(fo, "time", 0.0);
                    synth::StaticSpectralFrame frame;
                    if(varToFrame(objectProperty(fo, "frame", {}), frame))
                        timeline.frames[(size_t)i] = frame;
                }
            }
            if(timeline.durationSeconds <= 0.0f)
                timeline.durationSeconds = timeline.timeSeconds[(size_t)timeline.frameCount - 1];
            return true;
        }
        synth::StaticSpectralFrame single;
        if(varToFrame(v, single))
        {
            timeline = synth::makeStaticTimeline(single);
            return true;
        }
        return false;
    }

    static juce::var parameterLockToVar(const synth::ParameterLock &lock)
    {
        auto *o = new juce::DynamicObject();
        o->setProperty("scope", (int)lock.scope);
        o->setProperty("enabled", lock.enabled);
        o->setProperty("domain", (int)lock.target.domain);
        o->setProperty("parameterId", juce::String(lock.target.parameterId));
        o->setProperty("partialIndex", lock.target.partialIndex);
        o->setProperty("groupIndex", lock.target.groupIndex);
        o->setProperty("valueType", (int)lock.value.type);
        o->setProperty("valueMode", (int)lock.value.mode);
        o->setProperty("floatValue", lock.value.floatValue);
        o->setProperty("intValue", lock.value.intValue);
        o->setProperty("boolValue", lock.value.boolValue);
        return juce::var(o);
    }

    static synth::ParameterLock varToParameterLock(const juce::var &v)
    {
        synth::ParameterLock lock;
        if(auto *o = v.getDynamicObject())
        {
            lock.scope = (synth::ParameterScope)juce::jlimit(0, 6, (int)objectProperty(o, "scope", 6));
            lock.enabled = (bool)objectProperty(o, "enabled", true);
            lock.target.domain = (synth::ParameterTargetDomain)juce::jlimit(0, 10, (int)objectProperty(o, "domain", 6));
            lock.target.parameterId = objectProperty(o, "parameterId", "").toString().toStdString();
            lock.target.partialIndex = (int)objectProperty(o, "partialIndex", -1);
            lock.target.groupIndex = (int)objectProperty(o, "groupIndex", -1);
            lock.value.type = (synth::LockValueType)juce::jlimit(0, 2, (int)objectProperty(o, "valueType", 0));
            lock.value.mode = (synth::LockValueMode)juce::jlimit(0, 2, (int)objectProperty(o, "valueMode", 0));
            lock.value.floatValue = (float)(double)objectProperty(o, "floatValue", 0.0);
            lock.value.intValue = (int)objectProperty(o, "intValue", 0);
            lock.value.boolValue = (bool)objectProperty(o, "boolValue", false);
        }
        return lock;
    }

    static juce::var compositionToVar(const synth::CompositionProject &project)
    {
        auto *root = new juce::DynamicObject();
        auto locksToVar = [](const std::vector<synth::ParameterLock> &locks)
        {
            juce::Array<juce::var> out;
            for(const auto &lock : locks) out.add(parameterLockToVar(lock));
            return out;
        };

        juce::Array<juce::var> seedPresets;
        for(const auto &seed : project.seedPresets)
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("id", (double)seed.id);
            o->setProperty("name", juce::String(seed.name));
            o->setProperty("kind", (int)seed.kind);
            o->setProperty("defaultLocks", locksToVar(seed.defaultLocks));
            seedPresets.add(juce::var(o));
        }
        root->setProperty("seedPresets", seedPresets);

        juce::Array<juce::var> creatorPresets;
        for(const auto &creator : project.creatorPresets)
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("id", (double)creator.id);
            o->setProperty("name", juce::String(creator.name));
            o->setProperty("kind", (int)creator.kind);
            o->setProperty("macroDefaults", locksToVar(creator.macroDefaults));
            juce::Array<juce::var> seeds;
            for(const auto &seed : creator.seedInstances)
            {
                auto *s = new juce::DynamicObject();
                s->setProperty("id", (double)seed.id);
                s->setProperty("seedPresetId", (double)seed.seedPresetId);
                s->setProperty("name", juce::String(seed.name));
                s->setProperty("overrides", locksToVar(seed.overrides));
                seeds.add(juce::var(s));
            }
            o->setProperty("seedInstances", seeds);
            creatorPresets.add(juce::var(o));
        }
        root->setProperty("creatorPresets", creatorPresets);

        juce::Array<juce::var> motifClips;
        for(const auto &clip : project.motifClips)
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("id", (double)clip.id);
            o->setProperty("name", juce::String(clip.name));
            o->setProperty("lengthBeats", clip.lengthBeats);
            o->setProperty("localAutomation", locksToVar(clip.localAutomation));
            juce::Array<juce::var> creators;
            for(const auto &creator : clip.creatorRefs)
            {
                auto *c = new juce::DynamicObject();
                c->setProperty("id", (double)creator.id);
                c->setProperty("creatorPresetId", (double)creator.creatorPresetId);
                c->setProperty("name", juce::String(creator.name));
                c->setProperty("overrides", locksToVar(creator.overrides));
                creators.add(juce::var(c));
            }
            o->setProperty("creatorRefs", creators);
            motifClips.add(juce::var(o));
        }
        root->setProperty("motifClips", motifClips);

        juce::Array<juce::var> structureEvents;
        for(const auto &event : project.structure.events)
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("id", (double)event.id);
            o->setProperty("motifClipId", (double)event.motifClipId);
            o->setProperty("startBeats", event.startBeats);
            o->setProperty("lengthBeats", event.lengthBeats);
            o->setProperty("transform", locksToVar(event.transform));
            structureEvents.add(juce::var(o));
        }
        auto *structure = new juce::DynamicObject();
        structure->setProperty("events", structureEvents);
        structure->setProperty("globalAutomation", locksToVar(project.structure.globalAutomation));
        root->setProperty("structure", juce::var(structure));

        auto channelsToVar = [&](const std::vector<synth::MixerChannel> &channels)
        {
            juce::Array<juce::var> out;
            for(const auto &channel : channels)
            {
                auto *o = new juce::DynamicObject();
                o->setProperty("id", (double)channel.id);
                o->setProperty("name", juce::String(channel.name));
                o->setProperty("sourceRef", (double)channel.sourceRef);
                o->setProperty("mixAutomation", locksToVar(channel.mixAutomation));
                out.add(juce::var(o));
            }
            return out;
        };
        auto *mixer = new juce::DynamicObject();
        mixer->setProperty("seedChannels", channelsToVar(project.mixer.seedChannels));
        mixer->setProperty("creatorChannels", channelsToVar(project.mixer.creatorChannels));
        mixer->setProperty("motifBuses", channelsToVar(project.mixer.motifBuses));
        mixer->setProperty("masterAutomation", locksToVar(project.mixer.masterAutomation));
        root->setProperty("mixer", juce::var(mixer));

        juce::Array<juce::var> motifs;
        for(const auto &motif : project.motifs)
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("id", (double)motif.id);
            o->setProperty("name", juce::String(motif.name));
            juce::Array<juce::var> notes;
            for(const auto &note : motif.noteEvents)
            {
                auto *n = new juce::DynamicObject();
                n->setProperty("id", (double)note.id);
                n->setProperty("midiNote", note.midiNote);
                n->setProperty("startBeats", note.startBeats);
                n->setProperty("durationBeats", note.durationBeats);
                n->setProperty("velocity", note.velocity);
                n->setProperty("sourceRef", juce::String(note.sourceRef));
                juce::Array<juce::var> locks;
                for(const auto &lock : note.parameterLocks) locks.add(parameterLockToVar(lock));
                n->setProperty("locks", locks);
                notes.add(juce::var(n));
            }
            o->setProperty("notes", notes);
            motifs.add(juce::var(o));
        }
        root->setProperty("motifs", motifs);

        juce::Array<juce::var> patterns;
        for(const auto &pattern : project.patterns)
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("id", (double)pattern.id);
            o->setProperty("name", juce::String(pattern.name));
            o->setProperty("lengthBeats", pattern.lengthBeats);
            juce::Array<juce::var> ids;
            for(auto id : pattern.motifIds) ids.add((double)id);
            o->setProperty("motifIds", ids);
            patterns.add(juce::var(o));
        }
        root->setProperty("patterns", patterns);

        juce::Array<juce::var> tracks;
        for(const auto &track : project.arrangeTracks)
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("id", (double)track.id);
            o->setProperty("name", juce::String(track.name));
            o->setProperty("previewMode", (int)track.previewMode);
            tracks.add(juce::var(o));
        }
        root->setProperty("arrangeTracks", tracks);

        juce::Array<juce::var> regions;
        for(const auto &region : project.arrangeRegions)
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("id", (double)region.id);
            o->setProperty("trackId", (double)region.trackId);
            o->setProperty("patternId", (double)region.patternId);
            o->setProperty("startBeats", region.startBeats);
            o->setProperty("lengthBeats", region.lengthBeats);
            regions.add(juce::var(o));
        }
        root->setProperty("arrangeRegions", regions);
        return juce::var(root);
    }

    static synth::CompositionProject varToComposition(const juce::var &v)
    {
        synth::CompositionProject project;
        auto *root = v.getDynamicObject();
        if(root == nullptr) return project;
        auto varToLocks = [](const juce::var &locks)
        {
            std::vector<synth::ParameterLock> out;
            if(locks.isArray())
                for(const auto &lv : *locks.getArray()) out.push_back(varToParameterLock(lv));
            return out;
        };

        if(auto arr = objectProperty(root, "seedPresets", {}); arr.isArray())
        {
            for(const auto &sv : *arr.getArray())
            {
                if(auto *o = sv.getDynamicObject())
                {
                    synth::SeedPreset seed;
                    seed.id = (uint64_t)(double)objectProperty(o, "id", 0.0);
                    seed.name = objectProperty(o, "name", "Seed").toString().toStdString();
                    seed.kind = (synth::SeedKind)juce::jlimit(0, 1, (int)objectProperty(o, "kind", 0));
                    seed.defaultLocks = varToLocks(objectProperty(o, "defaultLocks", {}));
                    project.seedPresets.push_back(std::move(seed));
                }
            }
        }
        if(auto arr = objectProperty(root, "creatorPresets", {}); arr.isArray())
        {
            for(const auto &cv : *arr.getArray())
            {
                if(auto *o = cv.getDynamicObject())
                {
                    synth::CreatorPreset creator;
                    creator.id = (uint64_t)(double)objectProperty(o, "id", 0.0);
                    creator.name = objectProperty(o, "name", "Creator").toString().toStdString();
                    creator.kind = (synth::CreatorKind)juce::jlimit(0, 3, (int)objectProperty(o, "kind", 0));
                    creator.macroDefaults = varToLocks(objectProperty(o, "macroDefaults", {}));
                    if(auto seeds = objectProperty(o, "seedInstances", {}); seeds.isArray())
                    {
                        for(const auto &sv : *seeds.getArray())
                        {
                            if(auto *s = sv.getDynamicObject())
                            {
                                synth::SeedInstance seed;
                                seed.id = (uint64_t)(double)objectProperty(s, "id", 0.0);
                                seed.seedPresetId = (uint64_t)(double)objectProperty(s, "seedPresetId", 0.0);
                                seed.name = objectProperty(s, "name", "Seed Instance").toString().toStdString();
                                seed.overrides = varToLocks(objectProperty(s, "overrides", {}));
                                creator.seedInstances.push_back(std::move(seed));
                            }
                        }
                    }
                    project.creatorPresets.push_back(std::move(creator));
                }
            }
        }
        if(auto arr = objectProperty(root, "motifClips", {}); arr.isArray())
        {
            for(const auto &mv : *arr.getArray())
            {
                if(auto *o = mv.getDynamicObject())
                {
                    synth::MotifClip clip;
                    clip.id = (uint64_t)(double)objectProperty(o, "id", 0.0);
                    clip.name = objectProperty(o, "name", "Motif").toString().toStdString();
                    clip.lengthBeats = (float)(double)objectProperty(o, "lengthBeats", 4.0);
                    clip.localAutomation = varToLocks(objectProperty(o, "localAutomation", {}));
                    if(auto creators = objectProperty(o, "creatorRefs", {}); creators.isArray())
                    {
                        for(const auto &cv : *creators.getArray())
                        {
                            if(auto *c = cv.getDynamicObject())
                            {
                                synth::CreatorInstance creator;
                                creator.id = (uint64_t)(double)objectProperty(c, "id", 0.0);
                                creator.creatorPresetId = (uint64_t)(double)objectProperty(c, "creatorPresetId", 0.0);
                                creator.name = objectProperty(c, "name", "Creator Instance").toString().toStdString();
                                creator.overrides = varToLocks(objectProperty(c, "overrides", {}));
                                clip.creatorRefs.push_back(std::move(creator));
                            }
                        }
                    }
                    project.motifClips.push_back(std::move(clip));
                }
            }
        }
        if(auto structureVar = objectProperty(root, "structure", {}); auto *structure = structureVar.getDynamicObject())
        {
            project.structure.globalAutomation = varToLocks(objectProperty(structure, "globalAutomation", {}));
            if(auto arr = objectProperty(structure, "events", {}); arr.isArray())
            {
                for(const auto &ev : *arr.getArray())
                {
                    if(auto *o = ev.getDynamicObject())
                    {
                        synth::StructureEvent event;
                        event.id = (uint64_t)(double)objectProperty(o, "id", 0.0);
                        event.motifClipId = (uint64_t)(double)objectProperty(o, "motifClipId", 0.0);
                        event.startBeats = (float)(double)objectProperty(o, "startBeats", 0.0);
                        event.lengthBeats = (float)(double)objectProperty(o, "lengthBeats", 4.0);
                        event.transform = varToLocks(objectProperty(o, "transform", {}));
                        project.structure.events.push_back(std::move(event));
                    }
                }
            }
        }
        if(auto mixerVar = objectProperty(root, "mixer", {}); auto *mixer = mixerVar.getDynamicObject())
        {
            auto readChannels = [&](const juce::var &channels)
            {
                std::vector<synth::MixerChannel> out;
                if(channels.isArray())
                {
                    for(const auto &cv : *channels.getArray())
                    {
                        if(auto *o = cv.getDynamicObject())
                        {
                            synth::MixerChannel channel;
                            channel.id = (uint64_t)(double)objectProperty(o, "id", 0.0);
                            channel.name = objectProperty(o, "name", "Channel").toString().toStdString();
                            channel.sourceRef = (uint64_t)(double)objectProperty(o, "sourceRef", 0.0);
                            channel.mixAutomation = varToLocks(objectProperty(o, "mixAutomation", {}));
                            out.push_back(std::move(channel));
                        }
                    }
                }
                return out;
            };
            project.mixer.seedChannels = readChannels(objectProperty(mixer, "seedChannels", {}));
            project.mixer.creatorChannels = readChannels(objectProperty(mixer, "creatorChannels", {}));
            project.mixer.motifBuses = readChannels(objectProperty(mixer, "motifBuses", {}));
            project.mixer.masterAutomation = varToLocks(objectProperty(mixer, "masterAutomation", {}));
        }
        if(auto arr = objectProperty(root, "motifs", {}); arr.isArray())
        {
            for(const auto &mv : *arr.getArray())
            {
                if(auto *o = mv.getDynamicObject())
                {
                    synth::MotifDefinition motif;
                    motif.id = (uint64_t)(double)objectProperty(o, "id", 0.0);
                    motif.name = objectProperty(o, "name", "Motif").toString().toStdString();
                    if(auto notes = objectProperty(o, "notes", {}); notes.isArray())
                    {
                        for(const auto &nv : *notes.getArray())
                        {
                            if(auto *n = nv.getDynamicObject())
                            {
                                synth::PianoNoteEvent note;
                                note.id = (uint64_t)(double)objectProperty(n, "id", 0.0);
                                note.midiNote = (int)objectProperty(n, "midiNote", 60);
                                note.startBeats = (float)(double)objectProperty(n, "startBeats", 0.0);
                                note.durationBeats = (float)(double)objectProperty(n, "durationBeats", 1.0);
                                note.velocity = (float)(double)objectProperty(n, "velocity", 0.8);
                                note.sourceRef = objectProperty(n, "sourceRef", "").toString().toStdString();
                                if(auto locks = objectProperty(n, "locks", {}); locks.isArray())
                                    for(const auto &lv : *locks.getArray()) note.parameterLocks.push_back(varToParameterLock(lv));
                                motif.noteEvents.push_back(std::move(note));
                            }
                        }
                    }
                    project.motifs.push_back(std::move(motif));
                }
            }
        }
        return project;
    }

    juce::var presetToVar() const
    {
        // Full patch state: source, operators, performance, matrix, LFO, and optional bake.
        auto gen = synthCore.getGeneratorParams();
        auto chain = synthCore.getOperatorChain();
        auto adsr = synthCore.getGlobalAdsr();
        auto lfos = synthCore.getLfoParamsSnapshot();
        auto rules = synthCore.getMatrixRulesSnapshot();
        auto chaos = synthCore.getChaosParams();
        auto shapeSource = synthCore.getShapeSourceParams();
        auto effectsParams = synthCore.getEffectsParams();
        auto resamplingParams = synthCore.getResamplingParams();
        auto postEffectsParams = synthCore.getPostResampleEffectsParams();
        auto composition = synthCore.getCompositionProject();

        auto *root = new juce::DynamicObject();
        root->setProperty("schema", "motifforge/0.3");
        root->setProperty("generatorType", generatorName(gen.type));
        // direct
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("partialCount", gen.direct.partialCount);
            root->setProperty("direct", juce::var(o));
        }
        {
            const auto &pre = gen.pre;
            auto *o = new juce::DynamicObject();
            o->setProperty("freqShape", freqShapeName(pre.freqShape));
            o->setProperty("inharm", pre.inharmonicAmount);
            o->setProperty("tilt", pre.tilt);
            o->setProperty("lambda", pre.lambda);
            o->setProperty("gamma", pre.gamma);
            o->setProperty("warpMode", warpName(pre.warpMode));
            o->setProperty("warpAmount", pre.warpAmount);
            o->setProperty("warpP", pre.warpP);
            o->setProperty("warpCenter", pre.warpCenter);
            o->setProperty("focusCenter", pre.focusCenter);
            o->setProperty("focusWidth", pre.focusWidth);
            o->setProperty("focusAmount", pre.focusAmount);
            o->setProperty("ampRandom", pre.ampRandom);
            o->setProperty("decaySpread", pre.decaySpread);
            o->setProperty("releaseSpread", pre.releaseSpread);
            o->setProperty("phaseInit", phaseInitName(pre.phaseInitMode));
            o->setProperty("phaseSeed", (int)pre.phaseSeed);
            root->setProperty("preprocessor", juce::var(o));
        }
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("partialCount", gen.modal.partialCount);
            o->setProperty("shape", modalShapeName(gen.modal.shape));
            o->setProperty("stiffness", gen.modal.stiffness);
            o->setProperty("dampingZeta", gen.modal.dampingZeta);
            o->setProperty("dampingHF", gen.modal.dampingHF);
            o->setProperty("pickup", gen.modal.pickupPosition);
            root->setProperty("modal", juce::var(o));
        }
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("partialCount", gen.pde.partialCount);
            o->setProperty("body", pdeBodyName(gen.pde.body));
            o->setProperty("param1", gen.pde.param1);
            o->setProperty("param2", gen.pde.param2);
            o->setProperty("dampingZeta", gen.pde.dampingZeta);
            o->setProperty("dampingHF", gen.pde.dampingHF);
            o->setProperty("pickup", gen.pde.pickupPosition);
            root->setProperty("pde", juce::var(o));
        }
        {
            auto *o = new juce::DynamicObject();
            const auto &fss = gen.functionalSource;
            o->setProperty("partialCount", fss.partialCount);
            o->setProperty("filePath", juce::String(fss.filePath));
            o->setProperty("minRootHz", fss.minRootHz);
            o->setProperty("maxRootHz", fss.maxRootHz);
            o->setProperty("userLockRoot", fss.userLockRoot);
            o->setProperty("userRootHz", fss.userRootHz);
            o->setProperty("attackSharpness", fss.attackSharpness);
            o->setProperty("brightnessDecay", fss.brightnessDecay);
            o->setProperty("bodyResonance", fss.bodyResonance);
            o->setProperty("quality", (int)fss.quality);
            o->setProperty("phaseSeed", (int)fss.phaseSeed);
            o->setProperty("transientAmount", fss.transientAmount);
            o->setProperty("residualAmount", fss.residualAmount);
            root->setProperty("functionalSource", juce::var(o));
        }
        {
            auto *o = new juce::DynamicObject();
            const auto &sp = gen.samplePlayback;
            o->setProperty("filePath", juce::String(sp.filePath));
            o->setProperty("loopEnabled", sp.loopEnabled);
            o->setProperty("reverse", sp.reverse);
            o->setProperty("rootMidi", sp.rootMidi);
            o->setProperty("start01", sp.start01);
            o->setProperty("end01", sp.end01);
            o->setProperty("playbackGain", sp.playbackGain);
            o->setProperty("pitchOffsetSemitones", sp.pitchOffsetSemitones);
            o->setProperty("attackMs", sp.attackMs);
            o->setProperty("releaseMs", sp.releaseMs);
            root->setProperty("samplePlayback", juce::var(o));
        }
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("voices", gen.unison.voices);
            o->setProperty("detuneCents", gen.unison.detuneCents);
            o->setProperty("widthStereo", gen.unison.widthStereo);
            o->setProperty("phaseSpread", gen.unison.phaseSpread);
            o->setProperty("phaseSeed", (int)gen.unison.phaseSeed);
            root->setProperty("unison", juce::var(o));
        }
        root->setProperty("partialMaxRefHz", gen.partialMaxRefHz);

        juce::Array<juce::var> opsArr;
        for(const auto &op : chain.ops)
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("enabled", op.enabled);
            o->setProperty("type", operatorTypeStr(op.type));
            o->setProperty("maskLow", op.maskLow);
            o->setProperty("maskHigh", op.maskHigh);
            o->setProperty("groupBits", (op.maskGroupLow ? 1 : 0)
                                        | (op.maskGroupMid ? 2 : 0)
                                        | (op.maskGroupHigh ? 4 : 0));
            o->setProperty("gainLow", op.gainLow);
            o->setProperty("gainMid", op.gainMid);
            o->setProperty("gainHigh", op.gainHigh);
            o->setProperty("jitterAmount", op.jitterAmount);
            o->setProperty("jitterSeed", (int)op.jitterSeed);
            o->setProperty("extraTilt", op.extraTilt);
            o->setProperty("lockAmount", op.lockAmount);
            opsArr.add(juce::var(o));
        }
        root->setProperty("operators", opsArr);

        juce::Array<juce::var> lfoArr;
        for(const auto &lfo : lfos)
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("enabled", lfo.enabled);
            o->setProperty("shape", lfoShapeName(lfo.shape));
            o->setProperty("frequencyHz", lfo.frequencyHz);
            o->setProperty("phase0", lfo.phase0);
            o->setProperty("rhoLfo", lfo.rhoLfo);
            o->setProperty("pUp", lfo.pUp);
            o->setProperty("pDown", lfo.pDown);
            lfoArr.add(juce::var(o));
        }
        root->setProperty("lfos", lfoArr);

        juce::Array<juce::var> ruleArr;
        for(const auto &rule : rules)
        {
            auto *o = new juce::DynamicObject();
            o->setProperty("enabled", rule.enabled);
            o->setProperty("source", (int)rule.source);
            o->setProperty("dest", (int)rule.dest);
            o->setProperty("depth", rule.depth);
            o->setProperty("weight", (int)rule.weight);
            o->setProperty("bandLo", rule.bandLo);
            o->setProperty("bandHi", rule.bandHi);
            ruleArr.add(juce::var(o));
        }
        root->setProperty("matrixRules", ruleArr);

        auto *shapeObj = new juce::DynamicObject();
        shapeObj->setProperty("shape", lfoShapeName(shapeSource.shape));
        shapeObj->setProperty("phase0", shapeSource.phase0);
        shapeObj->setProperty("rho", shapeSource.rho);
        shapeObj->setProperty("pUp", shapeSource.pUp);
        shapeObj->setProperty("pDown", shapeSource.pDown);
        shapeObj->setProperty("useSpectralX", shapeSource.useSpectralX);
        root->setProperty("shapeSource", juce::var(shapeObj));

        auto *chaosObj = new juce::DynamicObject();
        chaosObj->setProperty("enabled", chaos.enabled);
        chaosObj->setProperty("type", chaosNoiseName(chaos.type));
        chaosObj->setProperty("frequencyHz", chaos.frequencyHz);
        chaosObj->setProperty("amount", chaos.amount);
        root->setProperty("chaos", juce::var(chaosObj));

        auto *fxObj = new juce::DynamicObject();
        {
            auto *eq = new juce::DynamicObject();
            eq->setProperty("enabled", effectsParams.eq.enabled);
            eq->setProperty("mode", effectModeUiName(effectsParams.eq.mode));
            eq->setProperty("lowGainDb", effectsParams.eq.lowGainDb);
            eq->setProperty("midGainDb", effectsParams.eq.midGainDb);
            eq->setProperty("highGainDb", effectsParams.eq.highGainDb);
            eq->setProperty("drive", effectsParams.eq.drive);
            fxObj->setProperty("eq", juce::var(eq));
        }
        {
            auto *filter = new juce::DynamicObject();
            filter->setProperty("enabled", effectsParams.filter.enabled);
            filter->setProperty("mode", effectModeUiName(effectsParams.filter.mode));
            filter->setProperty("type", filterTypeUiName(effectsParams.filter.type));
            filter->setProperty("cutoffHz", effectsParams.filter.cutoffHz);
            filter->setProperty("resonance", effectsParams.filter.resonance);
            filter->setProperty("drive", effectsParams.filter.drive);
            fxObj->setProperty("filter", juce::var(filter));
        }
        root->setProperty("effects", juce::var(fxObj));

        {
            auto *rs = new juce::DynamicObject();
            rs->setProperty("enabled", resamplingParams.enabled);
            rs->setProperty("recording", false);
            rs->setProperty("playbackEnabled", resamplingParams.playbackEnabled);
            rs->setProperty("loopEnabled", resamplingParams.loopEnabled);
            rs->setProperty("reverse", resamplingParams.reverse);
            rs->setProperty("recordSeconds", resamplingParams.recordSeconds);
            rs->setProperty("dryWet", resamplingParams.dryWet);
            rs->setProperty("playbackGain", resamplingParams.playbackGain);
            rs->setProperty("pitchSemitones", resamplingParams.pitchSemitones);
            rs->setProperty("sliceStart", resamplingParams.sliceStart);
            rs->setProperty("sliceEnd", resamplingParams.sliceEnd);
            rs->setProperty("sliceCount", resamplingParams.sliceCount);
            rs->setProperty("sliceRotate", resamplingParams.sliceRotate);
            rs->setProperty("granularAmount", resamplingParams.granularAmount);
            rs->setProperty("grainSizeMs", resamplingParams.grainSizeMs);
            rs->setProperty("stutterAmount", resamplingParams.stutterAmount);
            rs->setProperty("stutterRateHz", resamplingParams.stutterRateHz);
            rs->setProperty("importedFilePath", juce::String(resamplingParams.importedFilePath));
            root->setProperty("resamplingEngine", juce::var(rs));
        }

        auto *fx2Obj = new juce::DynamicObject();
        {
            auto *eq = new juce::DynamicObject();
            eq->setProperty("enabled", postEffectsParams.eq.enabled);
            eq->setProperty("mode", effectModeUiName(postEffectsParams.eq.mode));
            eq->setProperty("lowGainDb", postEffectsParams.eq.lowGainDb);
            eq->setProperty("midGainDb", postEffectsParams.eq.midGainDb);
            eq->setProperty("highGainDb", postEffectsParams.eq.highGainDb);
            eq->setProperty("drive", postEffectsParams.eq.drive);
            fx2Obj->setProperty("eq", juce::var(eq));
        }
        {
            auto *filter = new juce::DynamicObject();
            filter->setProperty("enabled", postEffectsParams.filter.enabled);
            filter->setProperty("mode", effectModeUiName(postEffectsParams.filter.mode));
            filter->setProperty("type", filterTypeUiName(postEffectsParams.filter.type));
            filter->setProperty("cutoffHz", postEffectsParams.filter.cutoffHz);
            filter->setProperty("resonance", postEffectsParams.filter.resonance);
            filter->setProperty("drive", postEffectsParams.filter.drive);
            fx2Obj->setProperty("filter", juce::var(filter));
        }
        root->setProperty("postResampleEffects", juce::var(fx2Obj));
        root->setProperty("composition", compositionToVar(composition));

        auto *adsrObj = new juce::DynamicObject();
        adsrObj->setProperty("attack", adsr.attack);
        adsrObj->setProperty("decay", adsr.decay);
        adsrObj->setProperty("sustain", adsr.sustain);
        adsrObj->setProperty("release", adsr.release);
        adsrObj->setProperty("modeA", envCurveName(adsr.attackCurve));
        adsrObj->setProperty("modeD", envCurveName(adsr.decayCurve));
        adsrObj->setProperty("modeR", envCurveName(adsr.releaseCurve));
        adsrObj->setProperty("etaA", adsr.etaA);
        adsrObj->setProperty("etaD", adsr.etaD);
        adsrObj->setProperty("etaR", adsr.etaR);
        root->setProperty("adsr", juce::var(adsrObj));
        root->setProperty("globalGain", synthCore.getGlobalGain());
        return juce::var(root);
    }

    static synth::OperatorType strToOpType(const juce::String &s)
    {
        if(s == "AmpScalePerGroup") return synth::OperatorType::AmpScalePerGroup;
        if(s == "FrequencyJitter") return synth::OperatorType::FrequencyJitter;
        if(s == "SpectralTilt") return synth::OperatorType::SpectralTilt;
        if(s == "HarmonicLock") return synth::OperatorType::HarmonicLock;
        return synth::OperatorType::PartialMask;
    }

    bool varToPreset(const juce::var &root)
    {
        if(!root.isObject())
            return false;
        synth::SourceGenParams gen = synthCore.getGeneratorParams();
        gen.type = stringToGenerator(root.getProperty("generatorType", "DirectPartial").toString());
        if(auto *o = root.getProperty("direct", {}).getDynamicObject())
        {
            gen.direct.partialCount = (int)o->getProperty("partialCount");
            if(root.getProperty("preprocessor", {}).isVoid())
            {
                gen.pre.freqShape = stringToFreqShape(o->getProperty("freqShape").toString());
                gen.pre.inharmonicAmount = (float)(double)o->getProperty("inharm");
                gen.pre.tilt = (float)(double)o->getProperty("tilt");
                gen.pre.lambda = (float)(double)o->getProperty("lambda");
                gen.pre.gamma = (float)(double)o->getProperty("gamma");
                gen.pre.warpMode = stringToWarp(o->getProperty("warpMode").toString());
                gen.pre.warpAmount = (float)(double)o->getProperty("warpAmount");
                gen.pre.warpP = (float)(double)o->getProperty("warpP");
                gen.pre.warpCenter = (float)(double)o->getProperty("warpCenter");
                gen.pre.focusCenter = (float)(double)o->getProperty("focusCenter");
                gen.pre.focusWidth = (float)(double)o->getProperty("focusWidth");
                gen.pre.focusAmount = (float)(double)o->getProperty("focusAmount");
                gen.pre.ampRandom = (float)(double)o->getProperty("ampRandom");
                gen.pre.decaySpread = (float)(double)o->getProperty("decaySpread");
                gen.pre.releaseSpread = (float)(double)o->getProperty("releaseSpread");
                gen.pre.phaseInitMode = stringToPhaseInit(o->getProperty("phaseInit").toString());
                gen.pre.phaseSeed = (uint32_t)(int)o->getProperty("phaseSeed");
            }
        }
        if(auto *o = root.getProperty("preprocessor", {}).getDynamicObject())
        {
            gen.pre.freqShape = stringToFreqShape(o->getProperty("freqShape").toString());
            gen.pre.inharmonicAmount = (float)(double)o->getProperty("inharm");
            gen.pre.tilt = (float)(double)o->getProperty("tilt");
            gen.pre.lambda = (float)(double)o->getProperty("lambda");
            gen.pre.gamma = (float)(double)o->getProperty("gamma");
            gen.pre.warpMode = stringToWarp(o->getProperty("warpMode").toString());
            gen.pre.warpAmount = (float)(double)o->getProperty("warpAmount");
            gen.pre.warpP = (float)(double)o->getProperty("warpP");
            gen.pre.warpCenter = (float)(double)o->getProperty("warpCenter");
            gen.pre.focusCenter = (float)(double)o->getProperty("focusCenter");
            gen.pre.focusWidth = (float)(double)o->getProperty("focusWidth");
            gen.pre.focusAmount = (float)(double)o->getProperty("focusAmount");
            gen.pre.ampRandom = (float)(double)o->getProperty("ampRandom");
            gen.pre.decaySpread = (float)(double)o->getProperty("decaySpread");
            gen.pre.releaseSpread = (float)(double)o->getProperty("releaseSpread");
            gen.pre.phaseInitMode = stringToPhaseInit(o->getProperty("phaseInit").toString());
            gen.pre.phaseSeed = (uint32_t)(int)o->getProperty("phaseSeed");
        }
        if(auto *o = root.getProperty("modal", {}).getDynamicObject())
        {
            gen.modal.partialCount = (int)o->getProperty("partialCount");
            gen.modal.shape = stringToModalShape(o->getProperty("shape").toString());
            gen.modal.stiffness = (float)(double)o->getProperty("stiffness");
            gen.modal.dampingZeta = (float)(double)o->getProperty("dampingZeta");
            gen.modal.dampingHF = (float)(double)o->getProperty("dampingHF");
            gen.modal.pickupPosition = (float)(double)o->getProperty("pickup");
        }
        if(auto *o = root.getProperty("pde", {}).getDynamicObject())
        {
            gen.pde.partialCount = (int)o->getProperty("partialCount");
            gen.pde.body = stringToPDEBody(o->getProperty("body").toString());
            gen.pde.param1 = (float)(double)o->getProperty("param1");
            gen.pde.param2 = (float)(double)o->getProperty("param2");
            gen.pde.dampingZeta = (float)(double)o->getProperty("dampingZeta");
            gen.pde.dampingHF = (float)(double)o->getProperty("dampingHF");
            gen.pde.pickupPosition = (float)(double)o->getProperty("pickup");
        }
        // Accept both the new "functionalSource" key and the legacy
        // "samplePartialSet" key so v0.2 presets stay loadable.
        juce::var fssVar = root.getProperty("functionalSource", {});
        if(!fssVar.isObject()) fssVar = root.getProperty("samplePartialSet", {});
        if(auto *o = fssVar.getDynamicObject())
        {
            auto &fss = gen.functionalSource;
            fss.partialCount = (int)o->getProperty("partialCount");
            fss.filePath = o->getProperty("filePath").toString().toStdString();
            if(o->hasProperty("minRootHz")) fss.minRootHz = (float)(double)o->getProperty("minRootHz");
            if(o->hasProperty("maxRootHz")) fss.maxRootHz = (float)(double)o->getProperty("maxRootHz");
            if(o->hasProperty("userLockRoot")) fss.userLockRoot = (bool)o->getProperty("userLockRoot");
            if(o->hasProperty("userRootHz")) fss.userRootHz = (float)(double)o->getProperty("userRootHz");
            if(o->hasProperty("attackSharpness")) fss.attackSharpness = (float)(double)o->getProperty("attackSharpness");
            if(o->hasProperty("brightnessDecay")) fss.brightnessDecay = (float)(double)o->getProperty("brightnessDecay");
            if(o->hasProperty("bodyResonance"))   fss.bodyResonance   = (float)(double)o->getProperty("bodyResonance");
            if(o->hasProperty("quality"))
                fss.quality = (synth::FunctionalSampleQuality)(int)o->getProperty("quality");
            if(o->hasProperty("phaseSeed")) fss.phaseSeed = (uint32_t)(int)o->getProperty("phaseSeed");
            if(o->hasProperty("transientAmount")) fss.transientAmount = (float)(double)o->getProperty("transientAmount");
            if(o->hasProperty("residualAmount"))  fss.residualAmount  = (float)(double)o->getProperty("residualAmount");
        }
        if(auto *o = root.getProperty("samplePlayback", {}).getDynamicObject())
        {
            auto &sp = gen.samplePlayback;
            sp.filePath = objectProperty(o, "filePath", "").toString().toStdString();
            sp.loopEnabled = (bool)objectProperty(o, "loopEnabled", false);
            sp.reverse = (bool)objectProperty(o, "reverse", false);
            sp.rootMidi = (float)(double)objectProperty(o, "rootMidi", 60.0);
            sp.start01 = (float)(double)objectProperty(o, "start01", 0.0);
            sp.end01 = (float)(double)objectProperty(o, "end01", 1.0);
            sp.playbackGain = (float)(double)objectProperty(o, "playbackGain", 0.8);
            sp.pitchOffsetSemitones = (float)(double)objectProperty(o, "pitchOffsetSemitones", 0.0);
            sp.attackMs = (float)(double)objectProperty(o, "attackMs", 2.0);
            sp.releaseMs = (float)(double)objectProperty(o, "releaseMs", 40.0);
        }
        if(auto *o = root.getProperty("unison", {}).getDynamicObject())
        {
            gen.unison.voices = juce::jlimit(1, synth::kMaxUnison,
                                             (int)objectProperty(o, "voices", 1));
            gen.unison.detuneCents = (float)(double)objectProperty(o, "detuneCents", 12.0);
            gen.unison.widthStereo = (float)(double)objectProperty(o, "widthStereo", 0.7);
            gen.unison.phaseSpread = (float)(double)objectProperty(o, "phaseSpread", 1.0);
            gen.unison.phaseSeed = (uint32_t)(int)objectProperty(o, "phaseSeed", 17);
        }
        gen.partialMaxRefHz = (float)(double)root.getProperty("partialMaxRefHz", 110.0);

        synth::OperatorChain chain;
        if(auto opsArr = root.getProperty("operators", {}); opsArr.isArray())
        {
            for(const auto &v : *opsArr.getArray())
            {
                if(auto *o = v.getDynamicObject())
                {
                    synth::OperatorBase op;
                    op.enabled = (bool)o->getProperty("enabled");
                    op.type = strToOpType(o->getProperty("type").toString());
                    op.maskLow = (int)o->getProperty("maskLow");
                    op.maskHigh = (int)o->getProperty("maskHigh");
                    int bits = (int)o->getProperty("groupBits");
                    op.maskGroupLow = (bits & 1) != 0;
                    op.maskGroupMid = (bits & 2) != 0;
                    op.maskGroupHigh = (bits & 4) != 0;
                    op.gainLow = (float)(double)o->getProperty("gainLow");
                    op.gainMid = (float)(double)o->getProperty("gainMid");
                    op.gainHigh = (float)(double)o->getProperty("gainHigh");
                    op.jitterAmount = (float)(double)o->getProperty("jitterAmount");
                    op.jitterSeed = (uint32_t)(int)o->getProperty("jitterSeed");
                    op.extraTilt = (float)(double)o->getProperty("extraTilt");
                    op.lockAmount = (float)(double)o->getProperty("lockAmount");
                    chain.ops.push_back(op);
                }
            }
        }

        synth::GlobalAdsrParams adsr;
        if(auto *o = root.getProperty("adsr", {}).getDynamicObject())
        {
            adsr.attack = (float)(double)o->getProperty("attack");
            adsr.decay = (float)(double)o->getProperty("decay");
            adsr.sustain = (float)(double)o->getProperty("sustain");
            adsr.release = (float)(double)o->getProperty("release");
            adsr.attackCurve = stringToEnvCurve(o->getProperty("modeA").toString());
            adsr.decayCurve = stringToEnvCurve(o->getProperty("modeD").toString());
            adsr.releaseCurve = stringToEnvCurve(o->getProperty("modeR").toString());
            adsr.etaA = (float)(double)o->getProperty("etaA");
            adsr.etaD = (float)(double)o->getProperty("etaD");
            adsr.etaR = (float)(double)o->getProperty("etaR");
        }

        synthCore.setGeneratorParams(gen);
        if(!gen.samplePlayback.filePath.empty())
            synthCore.importSamplePlaybackFile(gen.samplePlayback.filePath);
        synthCore.setOperatorChain(chain);
        synthCore.setGlobalAdsr(adsr);
        synthCore.setGlobalGain((float)(double)root.getProperty("globalGain", 0.3));

        for(int i = 0; i < synth::kMaxLfos; ++i)
            synthCore.setLfoParams(i, synth::LfoParams {});
        if(auto lfoArr = root.getProperty("lfos", {}); lfoArr.isArray())
        {
            const int n = std::min(synth::kMaxLfos, lfoArr.getArray()->size());
            for(int i = 0; i < n; ++i)
            {
                if(auto *o = lfoArr.getArray()->getReference(i).getDynamicObject())
                {
                    synth::LfoParams p;
                    p.enabled = (bool)objectProperty(o, "enabled", false);
                    p.shape = stringToLfoShape(objectProperty(o, "shape", "Asymm").toString());
                    p.frequencyHz = (float)(double)objectProperty(o, "frequencyHz", 1.0);
                    p.phase0 = (float)(double)objectProperty(o, "phase0", 0.0);
                    p.rhoLfo = (float)(double)objectProperty(o, "rhoLfo", 0.5);
                    p.pUp = (float)(double)objectProperty(o, "pUp", 1.0);
                    p.pDown = (float)(double)objectProperty(o, "pDown", 1.0);
                    synthCore.setLfoParams(i, p);
                }
            }
        }

        for(int i = 0; i < synth::kMaxMatrixRules; ++i)
            synthCore.setMatrixRule(i, synth::MatrixRule {});
        if(auto ruleArr = root.getProperty("matrixRules", {}); ruleArr.isArray())
        {
            const int n = std::min(synth::kMaxMatrixRules, ruleArr.getArray()->size());
            for(int i = 0; i < n; ++i)
            {
                if(auto *o = ruleArr.getArray()->getReference(i).getDynamicObject())
                {
                    synth::MatrixRule r;
                    r.enabled = (bool)objectProperty(o, "enabled", false);
                    r.source = intToModSource((int)objectProperty(o, "source", 0));
                    r.dest = (synth::ModDestination)juce::jlimit(0, 3, (int)objectProperty(o, "dest", 0));
                    r.depth = (float)(double)objectProperty(o, "depth", 0.0);
                    r.weight = (synth::WeightMode)juce::jlimit(0, 6, (int)objectProperty(o, "weight", 0));
                    r.bandLo = (int)objectProperty(o, "bandLo", 0);
                    r.bandHi = (int)objectProperty(o, "bandHi", synth::kMaxPartials);
                    synthCore.setMatrixRule(i, r);
                }
            }
        }

        synth::ShapeSourceParams shapeSource;
        if(auto *o = root.getProperty("shapeSource", {}).getDynamicObject())
        {
            shapeSource.shape = stringToLfoShape(objectProperty(o, "shape", "Asymm").toString());
            shapeSource.phase0 = (float)(double)objectProperty(o, "phase0", 0.0);
            shapeSource.rho = (float)(double)objectProperty(o, "rho", 0.5);
            shapeSource.pUp = (float)(double)objectProperty(o, "pUp", 1.0);
            shapeSource.pDown = (float)(double)objectProperty(o, "pDown", 1.0);
            shapeSource.useSpectralX = (bool)objectProperty(o, "useSpectralX", false);
        }
        synthCore.setShapeSourceParams(shapeSource);

        synth::ChaosParams chaos;
        if(auto *o = root.getProperty("chaos", {}).getDynamicObject())
        {
            chaos.enabled = (bool)objectProperty(o, "enabled", false);
            chaos.type = stringToChaosNoise(objectProperty(o, "type", "Smooth").toString());
            chaos.frequencyHz = (float)(double)objectProperty(o, "frequencyHz", 8.0);
            chaos.amount = (float)(double)objectProperty(o, "amount", 1.0);
        }
        synthCore.setChaosParams(chaos);

        synth::EffectsChainParams effectsParams;
        if(auto *fx = root.getProperty("effects", {}).getDynamicObject())
        {
            if(auto *eq = objectProperty(fx, "eq", {}).getDynamicObject())
            {
                effectsParams.eq.enabled = (bool)objectProperty(eq, "enabled", false);
                effectsParams.eq.mode = stringToEffectMode(objectProperty(eq, "mode", "Normal").toString());
                effectsParams.eq.lowGainDb = (float)(double)objectProperty(eq, "lowGainDb", 0.0);
                effectsParams.eq.midGainDb = (float)(double)objectProperty(eq, "midGainDb", 0.0);
                effectsParams.eq.highGainDb = (float)(double)objectProperty(eq, "highGainDb", 0.0);
                effectsParams.eq.drive = (float)(double)objectProperty(eq, "drive", 1.0);
            }
            if(auto *filter = objectProperty(fx, "filter", {}).getDynamicObject())
            {
                effectsParams.filter.enabled = (bool)objectProperty(filter, "enabled", false);
                effectsParams.filter.mode = stringToEffectMode(objectProperty(filter, "mode", "Normal").toString());
                effectsParams.filter.type = stringToFilterType(objectProperty(filter, "type", "LowPass").toString());
                effectsParams.filter.cutoffHz = (float)(double)objectProperty(filter, "cutoffHz", 12000.0);
                effectsParams.filter.resonance = (float)(double)objectProperty(filter, "resonance", 0.0);
                effectsParams.filter.drive = (float)(double)objectProperty(filter, "drive", 1.0);
            }
        }
        synthCore.setEffectsParams(effectsParams);

        synth::ResamplingEngineParams resamplingParams;
        if(auto *rs = root.getProperty("resamplingEngine", {}).getDynamicObject())
        {
            resamplingParams.enabled = (bool)objectProperty(rs, "enabled", false);
            resamplingParams.recording = false;
            resamplingParams.playbackEnabled = (bool)objectProperty(rs, "playbackEnabled", false);
            resamplingParams.loopEnabled = (bool)objectProperty(rs, "loopEnabled", true);
            resamplingParams.reverse = (bool)objectProperty(rs, "reverse", false);
            resamplingParams.recordSeconds = (float)(double)objectProperty(rs, "recordSeconds", 8.0);
            resamplingParams.dryWet = (float)(double)objectProperty(rs, "dryWet", 1.0);
            resamplingParams.playbackGain = (float)(double)objectProperty(rs, "playbackGain", 0.8);
            resamplingParams.pitchSemitones = (float)(double)objectProperty(rs, "pitchSemitones", 0.0);
            resamplingParams.sliceStart = (float)(double)objectProperty(rs, "sliceStart", 0.0);
            resamplingParams.sliceEnd = (float)(double)objectProperty(rs, "sliceEnd", 1.0);
            resamplingParams.sliceCount = (int)objectProperty(rs, "sliceCount", 1);
            resamplingParams.sliceRotate = (int)objectProperty(rs, "sliceRotate", 0);
            resamplingParams.granularAmount = (float)(double)objectProperty(rs, "granularAmount", 0.0);
            resamplingParams.grainSizeMs = (float)(double)objectProperty(rs, "grainSizeMs", 80.0);
            resamplingParams.stutterAmount = (float)(double)objectProperty(rs, "stutterAmount", 0.0);
            resamplingParams.stutterRateHz = (float)(double)objectProperty(rs, "stutterRateHz", 8.0);
            resamplingParams.importedFilePath = objectProperty(rs, "importedFilePath", "").toString().toStdString();
        }
        synthCore.setResamplingParams(resamplingParams);
        if(!resamplingParams.importedFilePath.empty())
            synthCore.importResampleBuffer(resamplingParams.importedFilePath);

        synth::EffectsChainParams postEffectsParams;
        if(auto *fx = root.getProperty("postResampleEffects", {}).getDynamicObject())
        {
            if(auto *eq = objectProperty(fx, "eq", {}).getDynamicObject())
            {
                postEffectsParams.eq.enabled = (bool)objectProperty(eq, "enabled", false);
                postEffectsParams.eq.mode = stringToEffectMode(objectProperty(eq, "mode", "Normal").toString());
                postEffectsParams.eq.lowGainDb = (float)(double)objectProperty(eq, "lowGainDb", 0.0);
                postEffectsParams.eq.midGainDb = (float)(double)objectProperty(eq, "midGainDb", 0.0);
                postEffectsParams.eq.highGainDb = (float)(double)objectProperty(eq, "highGainDb", 0.0);
                postEffectsParams.eq.drive = (float)(double)objectProperty(eq, "drive", 1.0);
            }
            if(auto *filter = objectProperty(fx, "filter", {}).getDynamicObject())
            {
                postEffectsParams.filter.enabled = (bool)objectProperty(filter, "enabled", false);
                postEffectsParams.filter.mode = stringToEffectMode(objectProperty(filter, "mode", "Normal").toString());
                postEffectsParams.filter.type = stringToFilterType(objectProperty(filter, "type", "LowPass").toString());
                postEffectsParams.filter.cutoffHz = (float)(double)objectProperty(filter, "cutoffHz", 12000.0);
                postEffectsParams.filter.resonance = (float)(double)objectProperty(filter, "resonance", 0.0);
                postEffectsParams.filter.drive = (float)(double)objectProperty(filter, "drive", 1.0);
            }
        }
        synthCore.setPostResampleEffectsParams(postEffectsParams);
        synthCore.setCompositionProject(varToComposition(root.getProperty("composition", {})));

        generatorPanel.syncFromCore();
        operatorsPanel.refreshFromCore();
        matrixPanel.refreshFromCore();
        effectsPanel.refreshFromCore();
        resamplingPanel.refreshFromCore();
        postEffectsPanel.refreshFromCore();
        pianoLocksPanel.refreshFromCore();
        return true;
    }

    void savePreset()
    {
        chooser = std::make_unique<juce::FileChooser>("Save Preset",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.json");
        const auto cf = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;
        chooser->launchAsync(cf, [this](const juce::FileChooser &fc) {
            auto file = fc.getResult();
            if(file == juce::File {}) return;
            if(!file.hasFileExtension("json"))
                file = file.withFileExtension(".json");
            const auto json = juce::JSON::toString(presetToVar(), true);
            statusLabel.setText(file.replaceWithText(json) ? "Saved: " + file.getFileName() : "Save failed",
                                juce::dontSendNotification);
        });
    }
    void loadPreset()
    {
        chooser = std::make_unique<juce::FileChooser>("Load Preset",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.json");
        const auto cf = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        chooser->launchAsync(cf, [this](const juce::FileChooser &fc) {
            auto file = fc.getResult();
            if(file == juce::File {}) return;
            const auto parsed = juce::JSON::parse(file.loadFileAsString());
            statusLabel.setText(varToPreset(parsed) ? "Loaded: " + file.getFileName() : "Load failed",
                                juce::dontSendNotification);
        });
    }
    void showMidiDeviceDialog()
    {
        juce::PopupMenu menu;
        const auto devices = juce::MidiInput::getAvailableDevices();
        if(devices.isEmpty())
            menu.addItem(1, "(no MIDI inputs)", false);
        else
        {
            int id = 1;
            for(const auto &d : devices)
            {
                const bool en = deviceManager.isMidiInputDeviceEnabled(d.identifier);
                menu.addItem(id++, d.name, true, en);
            }
        }
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(midiDeviceButton),
                           [this, devices](int chosen) {
                               if(chosen <= 0 || chosen > devices.size()) return;
                               const auto &d = devices.getReference(chosen - 1);
                               const bool was = deviceManager.isMidiInputDeviceEnabled(d.identifier);
                               deviceManager.setMidiInputDeviceEnabled(d.identifier, !was);
                               if(!was)
                                   deviceManager.addMidiInputDeviceCallback(d.identifier, this);
                               else
                                   deviceManager.removeMidiInputDeviceCallback(d.identifier, this);
                               statusLabel.setText((was ? "Disabled MIDI: " : "Enabled MIDI: ") + d.name,
                                                   juce::dontSendNotification);
                           });
    }

    void showAudioSettingsDialog()
    {
        auto *selector = new juce::AudioDeviceSelectorComponent(deviceManager,
                                                                0, 0,
                                                                1, 2,
                                                                false, true,
                                                                true, false);
        selector->setSize(520, 420);

        juce::DialogWindow::LaunchOptions opts;
        opts.dialogTitle = "Audio Settings";
        opts.dialogBackgroundColour = juce::Colour(0xff15181b);
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = true;
        opts.resizable = true;
        opts.content.setOwned(selector);
        opts.launchAsync();
    }

    void refreshBufferSizeCombo()
    {
        const juce::ScopedValueSetter<bool> guard(suspendBufferSizeCallback, true);
        bufferSizeCombo.clear(juce::dontSendNotification);
        auto *device = deviceManager.getCurrentAudioDevice();
        if(device == nullptr)
        {
            bufferSizeCombo.addItem("-", 1);
            bufferSizeCombo.setSelectedId(1, juce::dontSendNotification);
            return;
        }

        const auto sizes = device->getAvailableBufferSizes();
        const int current = device->getCurrentBufferSizeSamples();
        int selectedId = 0;
        int id = 1;
        for(auto size : sizes)
        {
            bufferSizeCombo.addItem(juce::String(size), id);
            if(size == current)
                selectedId = id;
            ++id;
        }
        if(selectedId == 0)
        {
            bufferSizeCombo.addItem(juce::String(current), id);
            selectedId = id;
        }
        bufferSizeCombo.setSelectedId(selectedId, juce::dontSendNotification);
    }

    void applySelectedBufferSize()
    {
        if(suspendBufferSizeCallback)
            return;
        const int requested = bufferSizeCombo.getText().getIntValue();
        if(requested <= 0)
            return;

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.getAudioDeviceSetup(setup);
        setup.bufferSize = requested;
        const auto error = deviceManager.setAudioDeviceSetup(setup, true);
        statusLabel.setText(error.isEmpty()
                                ? "Buffer size: " + juce::String(requested)
                                : "Audio setup failed: " + error,
                            juce::dontSendNotification);
        refreshBufferSizeCombo();
    }

    synth::SynthCore synthCore;

    juce::TabbedComponent tabs;
    GeneratorPanel generatorPanel;
    OperatorsPanel operatorsPanel;
    MatrixPanel matrixPanel;
    EffectsPanel effectsPanel;
    ResamplingPanel resamplingPanel;
    EffectsPanel postEffectsPanel;
    PianoLocksPanel pianoLocksPanel;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent;
    juce::Label generalLabel;
    juce::Label statusLabel;
    juce::TextButton savePresetButton, loadPresetButton, midiDeviceButton, audioSettingsButton, panicButton;
    juce::Label bufferSizeLabel;
    juce::ComboBox bufferSizeCombo;
    bool suspendBufferSizeCallback = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

class MainWindow : public juce::DocumentWindow
{
  public:
    MainWindow(juce::String name)
        : DocumentWindow(std::move(name),
                         juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(
                             juce::ResizableWindow::backgroundColourId),
                         juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(), true);
        setResizable(true, true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }
    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class JuceAdditiveSynthApplication : public juce::JUCEApplication
{
  public:
    const juce::String getApplicationName() override { return "MotifForge v0.3"; }
    const juce::String getApplicationVersion() override { return "0.3.0"; }
    void initialise(const juce::String &) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }
    void shutdown() override { mainWindow.reset(); }

  private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(JuceAdditiveSynthApplication)

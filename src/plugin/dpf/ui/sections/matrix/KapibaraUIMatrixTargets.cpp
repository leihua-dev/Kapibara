#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

float KapibaraUI::modulationDepthLimit(synth::ModDestination destination)
{
        switch(destination)
        {
            case synth::ModDestination::PitchOct: return 4.0f;
            case synth::ModDestination::PitchSem: return 12.0f;
            case synth::ModDestination::PitchFine:
            case synth::ModDestination::PitchCrs: return 100.0f;
            case synth::ModDestination::Freq: return 2.0f;
            default: return 1.0f;
        }
    }

float KapibaraUI::defaultModulationDepth(synth::ModDestination destination)
{
        switch(destination)
        {
            case synth::ModDestination::PitchOct: return 1.0f;
            case synth::ModDestination::PitchSem: return 12.0f;
            case synth::ModDestination::PitchFine:
            case synth::ModDestination::PitchCrs: return 50.0f;
            case synth::ModDestination::Freq: return 0.25f;
            default: return 0.5f;
        }
    }

Color KapibaraUI::modulationSourceColor(synth::ModSource source)
{
        if(source >= synth::ModSource::Lfo1 && source <= synth::ModSource::Lfo4)
            return rgba(0x55c9ffff);
        if(source >= synth::ModSource::Env1 && source <= synth::ModSource::Env4)
            return rgba(0xffa84fff);
        if(source >= synth::ModSource::Adsr1 && source <= synth::ModSource::Adsr4)
            return rgba(0x6ee7a0ff);
        return rgba(0xb68cffff);
    }

ModRouteTarget KapibaraUI::modRouteTargetAt(float x, float y) const
{
        if(generator_.tracks.empty() || selectedTrack_ < 0 || selectedTrack_ >= int(generator_.tracks.size()))
            return {};
        const auto &track = generator_.tracks[(size_t)selectedTrack_];
        const auto make = [&](const Rect &rect, synth::ModDestination destination, const char *name) {
            return rect.w > 0.0f && rect.contains(x, y)
                       ? ModRouteTarget { true, destination, track.id, rect, name }
                       : ModRouteTarget {};
        };
        ModRouteTarget target;
        if((target = make(trackGainRect_, synth::ModDestination::TrackGain, "Gain")).valid) return target;
        if((target = make(trackPanRect_, synth::ModDestination::TrackPan, "Pan")).valid) return target;
        if((target = make(partialAmpRect_, synth::ModDestination::Amp, "Amp")).valid) return target;
        if((target = make(partialRatioRect_, synth::ModDestination::Freq, "Freq")).valid) return target;
        if((target = make(metaOctRect_, synth::ModDestination::PitchOct, "Oct")).valid) return target;
        if((target = make(metaSemRect_, synth::ModDestination::PitchSem, "Sem")).valid) return target;
        if((target = make(metaFinRect_, synth::ModDestination::PitchFine, "Fine")).valid) return target;
        if((target = make(metaCrsRect_, synth::ModDestination::PitchCrs, "Crs")).valid) return target;
        if((target = make(metaMorphRect_, synth::ModDestination::MetaMorph, "Morph")).valid) return target;
        if((target = make(metaWarpAmountRect_, synth::ModDestination::MetaWarp, "Warp")).valid) return target;
        if((target = make(metaPhaseRect_, synth::ModDestination::Phase, "Phase")).valid) return target;
        if((target = make(metaPanRect_, synth::ModDestination::MetaPan, "Pan")).valid) return target;
        (void)track;
        for(const auto &h : fxKnobHits_)
            if(h.trackId >= 0 && h.rect.w > 0.0f && h.rect.contains(x, y))
            {
                ModRouteTarget t;
                t.valid = true;
                t.destination = static_cast<synth::ModDestination>(int(synth::ModDestination::InsertP0) + h.knob);
                t.trackId = uint32_t(h.trackId);
                t.rect = h.rect;
                t.name = destName(t.destination);
                t.slot = h.insertIdx;
                return t;
            }
        return {};
    }

Rect KapibaraUI::modulationDestinationRect(const synth::MatrixRule &rule) const
{
        switch(rule.dest)
        {
            case synth::ModDestination::TrackGain: return trackGainRect_;
            case synth::ModDestination::TrackPan: return trackPanRect_;
            case synth::ModDestination::MetaPan: return metaPanRect_;
            case synth::ModDestination::PitchOct: return metaOctRect_;
            case synth::ModDestination::PitchSem: return metaSemRect_;
            case synth::ModDestination::PitchFine: return metaFinRect_;
            case synth::ModDestination::PitchCrs: return metaCrsRect_;
            case synth::ModDestination::MetaMorph: return metaMorphRect_;
            case synth::ModDestination::MetaWarp: return metaWarpAmountRect_;
            case synth::ModDestination::Amp: return partialAmpRect_;
            case synth::ModDestination::Freq: return partialRatioRect_;
            case synth::ModDestination::Phase: return metaPhaseRect_;
            case synth::ModDestination::OscModDepth: return basicModDepthRect_;
            case synth::ModDestination::InsertP0:
            case synth::ModDestination::InsertP1:
            case synth::ModDestination::InsertP2:
            case synth::ModDestination::InsertP3:
            {
                const int param = int(rule.dest) - int(synth::ModDestination::InsertP0);
                for(const auto &h : fxKnobHits_)
                    if(h.trackId == int(rule.targetTrackId) && h.insertIdx == rule.targetSlot && h.knob == param)
                        return h.rect;
                return {};
            }
            default: return {};
        }
    }

END_NAMESPACE_DISTRHO

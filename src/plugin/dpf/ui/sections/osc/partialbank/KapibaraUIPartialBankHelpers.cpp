#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::ensurePartialBankFrameDefaults(synth::WavetableSeedParams &seed, int frameIndex)
{
        seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
        frameIndex = clampi(frameIndex, 0, seed.frameCount - 1);
        auto &frame = seed.frames[(size_t)frameIndex];
        for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
        {
            auto &h = frame.harmonics[(size_t)i];
            if(h.ratio <= 0.0f)
                h.ratio = float(i + 1);
            // If the frame has never been written, mirror the legacy partial
            // amp/phase once so old presets still sound the same.
            if(std::abs(h.amp) <= 1.0e-8f && std::abs(h.phase) <= 1.0e-8f && seed.partials[(size_t)i].amp > 0.0f)
            {
                h.amp = seed.partials[(size_t)i].amp;
                h.phase = seed.partials[(size_t)i].phase;
            }
        }
    }

int KapibaraUI::partialBankMorphFrameIndex(const synth::WavetableSeedParams &seed)
{
        const int fc = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
        return fc > 1 ? clampi(int(seed.morph * float(fc - 1) + 0.5f), 0, fc - 1) : 0;
    }

float KapibaraUI::partialBankPitchRatio(int oct, int sem, float fin, float crs)
{
        const float totalSemis = float(oct) * 12.0f + float(sem) + fin / 100.0f + crs / 100.0f;
        return std::pow(2.0f, totalSemis / 12.0f);
    }

void KapibaraUI::applyPartialBankGroupPitch(synth::WavetableSeedParams &seed,
                                       int oct, int sem, float fin, float crs)
{
        const float pitchRatio = partialBankPitchRatio(oct, sem, fin, crs);
        for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
        {
            auto &slot = seed.partials[(size_t)i];
            slot.pitchOct = oct;
            slot.pitchSem = sem;
            slot.pitchFin = fin;
            slot.pitchCrs = crs;
            slot.ratio = float(i + 1) * pitchRatio;
        }
    }

bool KapibaraUI::trackGroupPitch(const synth::SourceTrackParams &t, TrackPitch &out)
{
        switch(t.type)
        {
            case synth::SourceTrackType::MetaOscillator:
                out = { t.metaOsc.pitchOct, t.metaOsc.pitchSem, t.metaOsc.pitchFin, t.metaOsc.pitchCrs };
                return true;
            case synth::SourceTrackType::PartialBank:
            {
                const auto &s = t.partialBank.partials[0];
                out = { s.pitchOct, s.pitchSem, s.pitchFin, s.pitchCrs };
                return true;
            }
            case synth::SourceTrackType::BasicOscillator:
                out = { t.basicPitchOct, t.basicPitchSem, t.basicPitchFin, t.basicPitchCrs };
                return true;
            default:
                return false;  // Sample / Noise has no harmonic series to shift
        }
    }

void KapibaraUI::applyTrackGroupPitch(synth::SourceTrackParams &t, const TrackPitch &p)
{
        const int oct = clampi(p.oct, -4, 4);
        const int sem = clampi(p.sem, -12, 12);
        const float fin = clampf(p.fin, -100.0f, 100.0f);
        const float crs = clampf(p.crs, -100.0f, 100.0f);
        switch(t.type)
        {
            case synth::SourceTrackType::MetaOscillator:
                t.metaOsc.pitchOct = oct;
                t.metaOsc.pitchSem = sem;
                t.metaOsc.pitchFin = fin;
                t.metaOsc.pitchCrs = crs;
                t.metaOsc.syncRatioFromPitch();
                break;
            case synth::SourceTrackType::PartialBank:
                applyPartialBankGroupPitch(t.partialBank, oct, sem, fin, crs);
                break;
            case synth::SourceTrackType::BasicOscillator:
                // The seed is regenerated from these on every rebuild
                // (dsp/BasicOscDsp.cpp buildBasicSeed), so storing them is enough.
                t.basicPitchOct = oct;
                t.basicPitchSem = sem;
                t.basicPitchFin = fin;
                t.basicPitchCrs = crs;
                break;
            default:
                break;
        }
    }

Kwt2PackedBin KapibaraUI::packKwtBin(float amp, float phase)
{
        Kwt2PackedBin bin;
        bin.amplitude = uint16_t(std::round(clampf(amp, 0.0f, 1.0f) * 65535.0f));
        while(phase > kPi) phase -= 2.0f * kPi;
        while(phase < -kPi) phase += 2.0f * kPi;
        bin.phase = int16_t(std::round(clampf(phase / kPi, -1.0f, 1.0f) * 32767.0f));
        return bin;
    }

synth::WavetableHarmonic KapibaraUI::unpackKwtBin(const Kwt2PackedBin &bin, int index)
{
        synth::WavetableHarmonic h;
        h.ratio = float(index + 1);
        h.amp = float(bin.amplitude) / 65535.0f;
        h.phase = float(bin.phase) / 32767.0f * kPi;
        return h;
    }

void KapibaraUI::applyFramePreset(int preset)
    {
        auto *track = currentTrack();
        if(!track) return;

        const bool isMeta = track->type == synth::SourceTrackType::MetaOscillator;
        const bool isBank = track->type == synth::SourceTrackType::PartialBank;
        if(!isMeta && !isBank) return;

        auto &storage = isMeta ? track->metaOsc.frames : track->partialBank.frames;
        const int frameCount = isMeta
            ? std::max(1, std::min(track->metaOsc.frameCount, synth::kMaxWavetableFrames))
            : std::max(1, std::min(track->partialBank.frameCount, synth::kMaxWavetableFrames));
        const int frameIdx = std::max(0, std::min(selectedMetaFrame_, frameCount - 1));

        auto &frame = storage[(size_t)frameIdx];
        frame.harmonics.fill(synth::WavetableHarmonic{});
        constexpr int N = synth::kEditableWavetableHarmonics;
        for(int i = 0; i < N; ++i)
            frame.harmonics[(size_t)i].ratio = float(i + 1);

        // Fourier normalization factors so each reconstruction peaks at ~1 (real
        // waveform level), instead of the raw 1/n sums that overshoot to ~pi/2. The
        // display shows true amplitude (no display-side normalization), so authoring
        // the presets at unit peak is what makes them read correctly.
        switch(preset)
        {
            case 0: // Sine
                frame.harmonics[0].amp = 1.0f;
                break;
            case 1: // Saw  (2/pi) * sum 1/n
                for(int i = 0; i < N; ++i)
                    frame.harmonics[(size_t)i].amp = (2.0f / kPi) / float(i + 1);
                break;
            case 2: // Square  (4/pi) * sum_odd 1/n
                for(int i = 0; i < N; i += 2)
                    frame.harmonics[(size_t)i].amp = (4.0f / kPi) / float(i + 1);
                break;
            case 3: // Triangle  (8/pi^2) * sum_odd (-1)^k / n^2
                for(int i = 0; i < N; i += 2)
                {
                    const float sign = ((i / 2) % 2 == 0) ? 1.0f : -1.0f;
                    frame.harmonics[(size_t)i].amp = (8.0f / (kPi * kPi)) * sign / float((i + 1) * (i + 1));
                }
                break;
            default: // Clear (preset 4)
                break;
        }

        pushCurrentTrack();
    }

END_NAMESPACE_DISTRHO

#include "../../KapibaraUI.hpp"

#include <fstream>
#include <sstream>
#include <type_traits>

START_NAMESPACE_DISTRHO

namespace
{
std::string hexEncode(const void *d, size_t n)
{
        static const char *H = "0123456789abcdef";
        const unsigned char *p = static_cast<const unsigned char *>(d);
        std::string s; s.reserve(n * 2);
        for(size_t i = 0; i < n; ++i) { s.push_back(H[p[i] >> 4]); s.push_back(H[p[i] & 0xf]); }
        return s;
}
void hexDecode(const std::string &s, void *out, size_t n)
{
        unsigned char *p = static_cast<unsigned char *>(out);
        const auto nib = [](char c) -> int { return (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : 0; };
        for(size_t i = 0; i < n && i * 2 + 1 < s.size(); ++i)
            p[i] = uint8_t((nib(s[i * 2]) << 4) | nib(s[i * 2 + 1]));
}
// Hex a trivially-copyable POD param struct. (InsertEffect can't be blob-copied
// whole because ConvSlotParams holds a shared_ptr + std::string.)
template <typename T> std::string hexPod(const T &v)
{
        static_assert(std::is_trivially_copyable<T>::value, "hexPod requires a POD struct");
        return hexEncode(&v, sizeof(T));
}
template <typename T> void unhexPod(const std::string &s, T &v)
{
        static_assert(std::is_trivially_copyable<T>::value, "unhexPod requires a POD struct");
        hexDecode(s, &v, sizeof(T));
}
// Wavetable payloads go in base64, not hex: at 1024 bins x 4 bytes a frame,
// hex's 2x blowup is the difference between a readable preset and a huge one.
const char *const kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string b64Encode(const void *d, size_t n)
{
        const unsigned char *p = static_cast<const unsigned char *>(d);
        std::string s;
        s.reserve((n + 2) / 3 * 4);
        for(size_t i = 0; i < n; i += 3)
        {
            const unsigned a0 = p[i];
            const unsigned a1 = (i + 1 < n) ? p[i + 1] : 0u;
            const unsigned a2 = (i + 2 < n) ? p[i + 2] : 0u;
            const unsigned v = (a0 << 16) | (a1 << 8) | a2;
            s.push_back(kB64[(v >> 18) & 0x3f]);
            s.push_back(kB64[(v >> 12) & 0x3f]);
            s.push_back(i + 1 < n ? kB64[(v >> 6) & 0x3f] : '=');
            s.push_back(i + 2 < n ? kB64[v & 0x3f] : '=');
        }
        return s;
}
// Returns bytes written. Anything unexpected yields a short count, which the
// caller treats as "corrupt payload, leave the frame alone" — a half-decoded
// wavetable would be worse than the default one.
size_t b64Decode(const std::string &s, void *out, size_t maxBytes)
{
        static int rev[256];
        static const bool init = [] {
            for(int i = 0; i < 256; ++i) rev[i] = -1;
            for(int i = 0; i < 64; ++i) rev[(unsigned char)kB64[i]] = i;
            return true;
        }();
        (void)init;
        unsigned char *p = static_cast<unsigned char *>(out);
        size_t written = 0;
        unsigned acc = 0;
        int have = 0;
        for(const char ch : s)
        {
            if(ch == '=') break;
            const int v = rev[(unsigned char)ch];
            if(v < 0) return written;   // stray character: stop, report short
            acc = (acc << 6) | unsigned(v);
            have += 6;
            if(have >= 8)
            {
                have -= 8;
                if(written >= maxBytes) return written;
                p[written++] = (unsigned char)((acc >> have) & 0xff);
            }
        }
        return written;
}
} // namespace

// Wavetable frames ride the preset as base64 of KWT2-packed bins — the exact
// 16-bit amp/phase packing the .kwt files use, so one encoder serves both.
// Trailing all-zero bins are dropped: a table using 64 of its 1024 harmonic
// slots stores 64. That trimming is what keeps a multi-track patch from
// exploding — the legacy text form spent ~6 KB on a frame that was mostly zeros,
// and that was for ONE global oscillator, not one per track.
std::string KapibaraUI::encodeFrameBins(const synth::WavetableFrameStorage &frames, int frameIndex,
                                        int binLimit, int &binCountOut) const
{
        binCountOut = 0;
        if(frameIndex < 0 || frameIndex >= synth::kMaxWavetableFrames)
            return {};
        const auto &fp = frames.get()[(size_t)frameIndex];
        if(!fp)
            return {};
        const auto &frame = *fp;
        const int limit = clampi(binLimit, 1, synth::kMaxWavetableHarmonics);
        int last = -1;
        for(int b = 0; b < limit; ++b)
            if(frame.harmonics[(size_t)b].amp != 0.0f || frame.harmonics[(size_t)b].phase != 0.0f)
                last = b;
        if(last < 0)
            return {};   // silent frame: no line at all
        const int n = last + 1;
        std::vector<Kwt2PackedBin> bins((size_t)n);
        for(int b = 0; b < n; ++b)
            bins[(size_t)b] = packKwtBin(frame.harmonics[(size_t)b].amp,
                                         frame.harmonics[(size_t)b].phase);
        binCountOut = n;
        return b64Encode(bins.data(), bins.size() * sizeof(Kwt2PackedBin));
    }

void KapibaraUI::decodeFrameBins(synth::WavetableFrameStorage &frames, int frameIndex,
                                 int binCount, const std::string &payload) const
{
        if(frameIndex < 0 || frameIndex >= synth::kMaxWavetableFrames)
            return;
        const int n = clampi(binCount, 0, synth::kMaxWavetableHarmonics);
        if(n <= 0)
            return;
        std::vector<Kwt2PackedBin> bins((size_t)n);
        const size_t want = bins.size() * sizeof(Kwt2PackedBin);
        if(b64Decode(payload, bins.data(), want) != want)
            return;   // corrupt: leave the frame at its default rather than half-fill it
        auto &arr = frames.ensure();
        if(!arr[(size_t)frameIndex])
            arr[(size_t)frameIndex] = std::make_shared<synth::WavetableFrame>();
        auto &frame = *arr[(size_t)frameIndex];
        frame.useImportedWaveform = false;
        frame.waveform.reset();
        frame.spectrum.reset();
        frame.harmonics.fill(synth::WavetableHarmonic {});
        for(int b = 0; b < n; ++b)
            frame.harmonics[(size_t)b] = unpackKwtBin(bins[(size_t)b], b);
    }

// Modern-state persistence: the legacy preset (plugin side) doesn't save the
// multi-track routing/structure, so the UI appends it here and parses it back.
void KapibaraUI::saveModernState(const std::string &path)
{
        std::ofstream out(path, std::ios::app);
        if(!out) return;
        writeModernState(out);
    }

std::string KapibaraUI::modernStateString()
{
        std::ostringstream out;
        writeModernState(out);
        return out.str();
    }

// structureOnly = a ROUTER preset: the rack and its wiring, without the sound
// design. Everything a synth's architecture is (which sources exist, how they
// are wired, what sits in each chain) and nothing that is a particular patch
// (MOD curves, matrix rules, mask groups). Loading one rebuilds the layout and
// leaves the modulation alone.
void KapibaraUI::writeModernState(std::ostream &out, bool structureOnly)
{
        out << "modern 1\n";
        for(size_t ti = 0; ti < generator_.tracks.size(); ++ti)
        {
            const auto &t = generator_.tracks[ti];
            out << "mtrack " << ti << ' ' << t.id << ' ' << int(t.type) << ' ' << t.gain << ' ' << t.pan << ' ' << t.send
                << ' ' << t.ampEnvIndex << ' ' << int(t.mute) << ' ' << int(t.solo)
                << ' ' << t.unison.voices << ' ' << t.unison.detuneCents << ' ' << t.unison.widthStereo << ' ' << t.unison.phaseSpread
                << ' ' << int(t.basicUnits[0].shape) << ' ' << t.basicUnits[0].pulseWidth
                << ' ' << t.basicUnits[0].subLevel << ' ' << int(t.sampleNoiseMode) << ' ' << t.noiseColor
                << ' ' << t.partialBank.partialCount << ' ' << t.perVoiceFilterCount << ' ' << t.inserts.size()
                << ' ' << t.basicUnits[0].pitchOct << ' ' << t.basicUnits[0].pitchSem
                << ' ' << t.basicUnits[0].pitchFin << ' ' << t.basicUnits[0].pitchCrs << "\n";
            // Full oscillator rack. Unit 0 also rides the legacy mtrack fields
            // above so an older build still loads something that sounds right.
            for(int u = 0; u < synth::kBasicOscUnits; ++u)
            {
                const auto &bu = t.basicUnits[(size_t)u];
                out << "mbosc " << ti << ' ' << u << ' ' << int(bu.enabled) << ' ' << int(bu.shape)
                    << ' ' << bu.pulseWidth << ' ' << bu.subLevel << ' ' << bu.level
                    << ' ' << bu.pitchOct << ' ' << bu.pitchSem
                    << ' ' << bu.pitchFin << ' ' << bu.pitchCrs << "\n";
            }
            if(t.basicMod.mode != synth::BasicOscModMode::Off || t.basicMod.depth != 0.0f)
                out << "mbmod " << ti << ' ' << int(t.basicMod.mode) << ' ' << int(t.basicMod.source)
                    << ' ' << int(t.basicMod.target) << ' ' << t.basicMod.depth << "\n";
            if(t.type == synth::SourceTrackType::SampleNoise)
            {
                const auto &sp = t.sampler;
                out << "msmp " << ti << ' ' << sp.rootNote << ' ' << int(sp.keyTrack) << ' '
                    << int(sp.loopMode) << ' ' << sp.startNorm << ' ' << sp.endNorm << ' '
                    << sp.loopStartNorm << ' ' << sp.loopEndNorm << ' ' << sp.sliceCount << ' '
                    << sp.gain << ' ' << int(sp.reverse) << ' ' << int(t.noiseType) << ' '
                    << sp.pitchOct << ' ' << sp.pitchSem << ' '
                    << sp.pitchFin << ' ' << sp.pitchCrs << "\n";
                // Audio is not embedded — the path is re-read on load, same as an
                // impulse response. A moved file simply comes back empty.
                if(!sp.samplePath.empty())
                    out << "msmpf " << ti << ' ' << sp.samplePath << "\n";
            }
            // Oscillator CONTENT (as opposed to its parameters). Skipped for a
            // router preset: that file is the architecture, and a wavetable is
            // the single largest thing in a patch. A full preset must carry it —
            // without these lines the track list restored below is built from
            // default-constructed tracks, so loading a preset actively wiped
            // every Partial Bank and Meta wavetable it was meant to restore.
            if(!structureOnly && t.type == synth::SourceTrackType::MetaOscillator)
            {
                const auto &m = t.metaOsc;
                out << "mmeta " << ti << ' ' << int(m.enabled) << ' ' << m.ratio << ' ' << m.amp
                    << ' ' << m.phase << ' ' << m.phaseRandom << ' ' << m.pan << ' ' << m.frameCount
                    << ' ' << m.morph << ' ' << int(m.warpMode) << ' ' << m.warpAmount
                    << ' ' << m.pitchOct << ' ' << m.pitchSem << ' ' << m.pitchFin << ' ' << m.pitchCrs << "\n";
                for(int f = 0; f < clampi(m.frameCount, 1, synth::kMaxWavetableFrames); ++f)
                {
                    int bins = 0;
                    const std::string payload = encodeFrameBins(m.frames, f, synth::kMaxWavetableHarmonics, bins);
                    if(bins > 0)
                        out << "mmetaf " << ti << ' ' << f << ' ' << bins << ' ' << payload << "\n";
                }
            }
            if(!structureOnly && t.type == synth::SourceTrackType::PartialBank)
            {
                const auto &b = t.partialBank;
                out << "mpb " << ti << ' ' << b.partialCount << ' ' << int(b.freqShape) << ' '
                    << b.inharmonicAmount << ' ' << b.frameCount << ' ' << b.morph << "\n";
                for(int f = 0; f < clampi(b.frameCount, 1, synth::kMaxWavetableFrames); ++f)
                {
                    int bins = 0;
                    // A bank frame maps only the 64 additive partials, not 1024 harmonics.
                    const std::string payload = encodeFrameBins(b.frames, f, synth::kMaxWavetablePartials, bins);
                    if(bins > 0)
                        out << "mpbf " << ti << ' ' << f << ' ' << bins << ' ' << payload << "\n";
                }
                for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
                {
                    const auto &p = b.partials[(size_t)i];
                    out << "mpbp " << ti << ' ' << i << ' ' << int(p.enabled) << ' ' << p.ratio
                        << ' ' << p.amp << ' ' << p.phase << ' ' << p.phaseRandom << ' ' << p.pan
                        << ' ' << p.frameCount << ' ' << p.morph << ' ' << int(p.warpMode)
                        << ' ' << p.warpAmount << ' ' << p.pitchOct << ' ' << p.pitchSem
                        << ' ' << p.pitchFin << ' ' << p.pitchCrs << "\n";
                    // Only the editable meta partials own frame tables, same bound
                    // the legacy `frame` key used.
                    if(i >= synth::kEditableMetaPartials)
                        continue;
                    for(int f = 0; f < clampi(p.frameCount, 1, synth::kMaxWavetableFrames); ++f)
                    {
                        int bins = 0;
                        const std::string payload = encodeFrameBins(p.frames, f, synth::kMaxWavetableHarmonics, bins);
                        if(bins > 0)
                            out << "mpbpf " << ti << ' ' << i << ' ' << f << ' ' << bins << ' ' << payload << "\n";
                    }
                }
            }
            out << "mtname " << ti << ' ' << t.name << "\n";
            for(int s = 0; s < t.perVoiceFilterCount && s < synth::kMaxPerVoiceFilters; ++s)
            {
                const auto &f = t.perVoiceFilters[(size_t)s];
                out << "mpvf " << ti << ' ' << s << ' ' << int(f.enabled) << ' ' << int(f.topology) << ' '
                    << f.cutoffHz << ' ' << f.resonance << ' ' << f.drive << ' ' << f.feedback << ' ' << f.mix << "\n";
            }
            for(int s = 0; s < synth::kMaxTrackMods; ++s)
            {
                const auto &m = t.mods[(size_t)s];
                if(!(m.enabled && m.sourceTrack >= 0))
                    continue;
                out << "mmod " << ti << ' ' << s << ' ' << int(m.sourceTrack) << ' ' << int(m.type) << ' '
                    << m.depth << ' ' << int(m.sourceKind) << ' ' << int(m.sourceNode) << "\n";
            }
            for(size_t ii = 0; ii < t.inserts.size(); ++ii)
            {
                const auto &e = t.inserts[ii];
                out << "mins " << ti << ' ' << ii << ' ' << int(e.kind) << ' ' << int(e.bypass)
                    << ' ' << hexPod(e.filter) << ' ' << hexPod(e.dist) << ' ' << hexPod(e.eq)
                    << ' ' << hexPod(e.comp) << ' ' << hexPod(e.delay) << ' ' << hexPod(e.reverb) << "\n";
                if(!e.conv.irName.empty())
                    out << "minsconv " << ti << ' ' << ii << ' ' << e.conv.irName << "\n"; // IR audio not persisted, only its name
            }
        }
        for(const auto &w : routeWires_)
            out << "mwire " << w.from.nodeId << ' ' << int(w.from.port) << ' ' << w.to.nodeId << ' ' << int(w.to.port) << "\n";
        // Matrix rules (legacy preset never stored them; without this they vanish
        // on save/load). The unconditional marker lets the loader distinguish "this
        // preset intentionally has zero routes" (clear everything) from "old preset
        // without a rules section" (preserve current rules).
        if(structureOnly)
        {
            // Node positions / port counts / util nodes / structure wires close
            // out the layout; matrix and MOD state is deliberately not written.
            writeModernStructureTail(out);
            return;
        }
        // The 8 MOD slots, Chaos and Shape were persisted NOWHERE — not here and
        // not in the legacy half — so every rule and mask group came back
        // referencing a default curve at a default rate.
        for(int i = 0; i < synth::kMaxModSlots; ++i)
        {
            const auto &m = modSlots_[(size_t)i];
            out << "mslot " << i << ' ' << int(m.enabled) << ' ' << int(m.loop) << ' '
                << m.rateHz << ' ' << clampi(m.pointCount, 2, synth::kMaxMatrixEnvPoints)
                << ' ' << int(m.tempoSync) << ' ' << int(m.syncDiv) << "\n";
            for(int k = 0; k < clampi(m.pointCount, 2, synth::kMaxMatrixEnvPoints); ++k)
                out << "mslotp " << i << ' ' << k << ' ' << m.points[(size_t)k].x << ' '
                    << m.points[(size_t)k].y << ' ' << m.points[(size_t)k].curve << "\n";
        }
        out << "mtempo " << uiTempoBpm_ << "\n";
        out << "mchaos " << int(chaos_.enabled) << ' ' << int(chaos_.type) << ' '
            << chaos_.frequencyHz << ' ' << chaos_.amount << "\n";
        out << "mshape " << int(shape_.shape) << ' ' << shape_.phase0 << ' ' << shape_.rho << ' '
            << shape_.pUp << ' ' << shape_.pDown << ' ' << int(shape_.useSpectralX) << "\n";
        out << "mrules 1\n";
        for(int ri = 0; ri < synth::kMaxMatrixRules; ++ri)
        {
            const auto &ru = rules_[(size_t)ri];
            if(!ru.enabled) continue;
            out << "mrule " << ri << ' ' << int(ru.source) << ' ' << int(ru.dest) << ' ' << ru.depth << ' '
                << int(ru.weight) << ' ' << ru.bandLo << ' ' << ru.bandHi << ' ' << ru.targetTrackId << ' '
                << ru.targetSlot << ' ' << int(ru.maskSlot) << ' ' << int(ru.maskAxis) << ' '
                << ru.transferCurve << ' ' << int(ru.muted) << "\n";
        }
        // Mask groups (advanced tier) ride the same mrules marker semantics.
        for(int gi = 0; gi < synth::kMaxMaskGroups; ++gi)
        {
            const auto &g = maskGroups_[(size_t)gi];
            // Persist by CONTENT, not by enabled — the ON toggle is a bypass, so
            // gating the write on it would silently discard a configured group's
            // whole setup once the user bypasses it.
            bool hasContent = g.enabled || g.family != 0 || g.waveSource != 0;
            for(const auto &t : g.targets)
                if(t.enabled) { hasContent = true; break; }
            if(!hasContent) continue;
            out << "mgrp " << gi << ' ' << int(g.baseSlot) << ' ' << g.freqSpread << ' '
                << g.phaseSpread << ' ' << g.spreadCurve << ' ' << int(g.family) << ' '
                << int(g.familyDest) << ' ' << g.familyTrackId << ' ' << g.familyDepth << ' '
                << int(g.enabled) << ' ' << int(g.waveSource) << ' ' << g.waveTrackId << ' '
                << g.rateHz << "\n";
            for(int k = 0; k < synth::kMaskGroupSlots; ++k)
            {
                const auto &t = g.targets[(size_t)k];
                if(!t.enabled) continue;
                out << "mgt " << gi << ' ' << k << ' ' << int(t.dest) << ' '
                    << t.targetTrackId << ' ' << t.depth << "\n";
            }
        }
        writeModernStructureTail(out);
    }

void KapibaraUI::writeModernStructureTail(std::ostream &out)
{
        for(const auto &kv : nodeOutPortCount_)
            out << "moutp " << kv.first << ' ' << kv.second << "\n";
        for(const auto &kv : structUtilCount_)
            out << "mutc " << kv.first << ' ' << kv.second << "\n";
        for(const auto &kv : structUtilParams_)
            out << "mutp " << kv.first << ' ' << kv.second.level << ' ' << kv.second.pan << ' '
                << kv.second.bandLoHz << ' ' << kv.second.bandHiHz << ' ' << int(kv.second.bandOn) << "\n";
        for(const auto &kv : structNodePos_)
            for(const auto &np : kv.second)
                out << "mnp " << kv.first << ' ' << np.first << ' ' << np.second.x << ' ' << np.second.y << "\n";
        for(const auto &kv : structWires_)
            for(const auto &w : kv.second)
                out << "msw " << kv.first << ' ' << w.from.nodeId << ' ' << int(w.from.port) << ' ' << w.to.nodeId << ' ' << int(w.to.port) << "\n";
        // Top-level board layout. mnp above is the layout INSIDE a focused node;
        // this is where the nodes sit on the main board. Without it the wires came
        // back but every node dropped to the auto-layout slot its draw pass hands
        // to routeNodePositions_.try_emplace, so a hand-arranged rack never
        // survived a reload.
        for(const auto &kv : routeNodePositions_)
            out << "mrnp " << kv.first << ' ' << kv.second.x << ' ' << kv.second.y << "\n";
        // Merge groups. Name is written last on its line so it may hold spaces.
        for(size_t gi = 0; gi < stripGroups_.size(); ++gi)
        {
            out << "msgn " << gi << ' ' << stripGroups_[gi].name << "\n";
            for(const int mi : stripGroups_[gi].memberIndices)
                out << "msgm " << gi << ' ' << mi << "\n";
        }
        // Which amp env each route-node instance drives. 255 = unassigned, which
        // is the fill state, so only real assignments are worth a line.
        for(int i = 0; i < synth::kMaxAmpEnvRouteNodes; ++i)
            if(ampEnvRouteNodeSlots_[(size_t)i] != 255)
                out << "maes " << i << ' ' << int(ampEnvRouteNodeSlots_[(size_t)i]) << "\n";
    }

void KapibaraUI::loadModernState(const std::string &path)
{
        std::ifstream in(path);
        if(!in) return;
        readModernState(in);
    }

void KapibaraUI::readModernState(std::istream &in)
{
        bool hasModern = false;
        std::vector<synth::SourceTrackParams> tracks;
        std::vector<synth::GridWire> wires;
        std::unordered_map<uint32_t, int> outPorts;
        std::unordered_map<uint32_t, int> utilCount;
        std::unordered_map<uint64_t, synth::RouteUtilParams> utilParams;
        std::unordered_map<uint32_t, std::unordered_map<int, synth::GridPoint>> nodePos;
        std::unordered_map<uint32_t, std::vector<synth::GridWire>> sWires;
        std::unordered_map<uint32_t, synth::GridPoint> boardPos;
        std::vector<StripGroup> groups;
        std::array<uint8_t, synth::kMaxAmpEnvRouteNodes> envSlots {};
        envSlots.fill(255);
        std::array<synth::MatrixRule, synth::kMaxMatrixRules> parsedRules {};
        std::array<synth::MaskGroup, synth::kMaxMaskGroups> parsedGroups {};
        bool hasRules = false;
        std::array<synth::ModSlotParams, synth::kMaxModSlots> parsedSlots {};
        synth::ChaosParams parsedChaos {};
        synth::ShapeSourceParams parsedShape {};
        bool hasSlots = false;
        const auto ensureTrack = [&](int ti) { if(ti >= 0 && ti >= int(tracks.size())) tracks.resize((size_t)ti + 1); };
        // Which tracks the file actually described oscillator CONTENT for. A
        // preset written before those tokens existed lists none, and its tracks
        // must keep the content already in memory rather than come back empty.
        std::vector<int> oscSeen;
        const auto markOsc = [&](int ti) {
            if(std::find(oscSeen.begin(), oscSeen.end(), ti) == oscSeen.end())
                oscSeen.push_back(ti);
        };
        std::string line;
        while(std::getline(in, line))
        {
            std::istringstream ss(line);
            std::string tok; ss >> tok;
            if(tok == "modern") { hasModern = true; continue; }
            if(!hasModern) continue;
            if(tok == "mtrack")
            {
                int ti; ss >> ti; ensureTrack(ti); if(ti < 0) continue;
                auto &t = tracks[(size_t)ti];
                int type, mute, solo, bshape, snmode, pc, pvfc, insc; unsigned id;
                ss >> id >> type >> t.gain >> t.pan >> t.send >> t.ampEnvIndex >> mute >> solo
                   >> t.unison.voices >> t.unison.detuneCents >> t.unison.widthStereo >> t.unison.phaseSpread
                   >> bshape >> t.basicUnits[0].pulseWidth >> t.basicUnits[0].subLevel
                   >> snmode >> t.noiseColor >> pc >> pvfc >> insc;
                t.id = id; t.type = synth::SourceTrackType(type); t.mute = mute; t.solo = solo;
                t.basicUnits[0].enabled = true;
                t.basicUnits[0].shape = synth::BasicOscillatorShape(bshape);
                t.sampleNoiseMode = synth::SampleNoiseMode(snmode);
                t.partialBank.partialCount = pc; t.perVoiceFilterCount = pvfc; t.inserts.clear();
                // Optional tail (basic-osc pitch), each re-defaulted on its own:
                // a failed >> writes 0 AND poisons the stream for later fields.
                int boct = 0, bsem = 0; float bfin = 0.0f, bcrs = 0.0f;
                if(!(ss >> boct)) boct = 0;
                if(!(ss >> bsem)) bsem = 0;
                if(!(ss >> bfin)) bfin = 0.0f;
                if(!(ss >> bcrs)) bcrs = 0.0f;
                t.basicUnits[0].pitchOct = clampi(boct, -4, 4);
                t.basicUnits[0].pitchSem = clampi(bsem, -12, 12);
                t.basicUnits[0].pitchFin = clampf(bfin, -100.0f, 100.0f);
                t.basicUnits[0].pitchCrs = clampf(bcrs, -100.0f, 100.0f);
            }
            else if(tok == "mtname")
            {
                int ti; ss >> ti; ensureTrack(ti); if(ti < 0) continue;
                std::string name; std::getline(ss, name);
                if(!name.empty() && name.front() == ' ') name.erase(name.begin());
                tracks[(size_t)ti].name = name;
            }
            else if(tok == "mpvf")
            {
                int ti, s, en, topo; ss >> ti >> s; ensureTrack(ti);
                if(ti < 0 || s < 0 || s >= synth::kMaxPerVoiceFilters) continue;
                auto &f = tracks[(size_t)ti].perVoiceFilters[(size_t)s];
                ss >> en >> topo >> f.cutoffHz >> f.resonance >> f.drive >> f.feedback >> f.mix;
                f.enabled = en; f.topology = synth::SourceFilterTopology(topo);
            }
            else if(tok == "msmp")
            {
                int ti, root, keyTrk, loop, slices, rev; float a, b, la, lb, gain;
                if(!(ss >> ti >> root >> keyTrk >> loop >> a >> b >> la >> lb >> slices >> gain >> rev))
                    continue;
                ensureTrack(ti);
                if(ti < 0) continue;
                auto &sp = tracks[(size_t)ti].sampler;
                sp.rootNote = clampi(root, 0, 127);
                sp.keyTrack = keyTrk != 0;
                sp.loopMode = synth::SampleLoopMode(clampi(loop, 0, 2));
                sp.startNorm = clampf(a, 0.0f, 1.0f);
                sp.endNorm = clampf(b, 0.0f, 1.0f);
                sp.loopStartNorm = clampf(la, 0.0f, 1.0f);
                sp.loopEndNorm = clampf(lb, 0.0f, 1.0f);
                sp.sliceCount = clampi(slices, 1, 64);
                sp.gain = clampf(gain, 0.0f, 2.0f);
                sp.reverse = rev != 0;
                // Optional tail, each re-defaulted on its own: a failed >> writes
                // 0 AND poisons the stream for every later field.
                int ntype = 0, poct = 0, psem = 0; float pfin = 0.0f, pcrs = 0.0f;
                if(!(ss >> ntype)) ntype = 0;
                if(!(ss >> poct)) poct = 0;
                if(!(ss >> psem)) psem = 0;
                if(!(ss >> pfin)) pfin = 0.0f;
                if(!(ss >> pcrs)) pcrs = 0.0f;
                tracks[(size_t)ti].noiseType = synth::NoiseType(clampi(ntype, 0, synth::kNoiseTypes - 1));
                sp.pitchOct = clampi(poct, -4, 4);
                sp.pitchSem = clampi(psem, -12, 12);
                sp.pitchFin = clampf(pfin, -100.0f, 100.0f);
                sp.pitchCrs = clampf(pcrs, -100.0f, 100.0f);
            }
            else if(tok == "msmpf")
            {
                int ti; ss >> ti;
                std::string path; std::getline(ss, path);
                if(!path.empty() && path.front() == ' ') path.erase(path.begin());
                ensureTrack(ti);
                if(ti < 0 || path.empty()) continue;
                tracks[(size_t)ti].sampler.samplePath = path;
            }
            else if(tok == "mbmod")
            {
                int ti, mode, src, dst; float depth;
                if(!(ss >> ti >> mode >> src >> dst >> depth))
                    continue;
                ensureTrack(ti);
                if(ti < 0) continue;
                auto &bm = tracks[(size_t)ti].basicMod;
                bm.mode = synth::BasicOscModMode(clampi(mode, 0, synth::kBasicOscModModes - 1));
                bm.source = uint8_t(clampi(src, 0, synth::kBasicOscUnits - 1));
                bm.target = uint8_t(clampi(dst, 0, synth::kBasicOscUnits - 1));
                bm.depth = clampf(depth, 0.0f, 1.0f);
            }
            else if(tok == "mbosc")
            {
                int ti, u, en, shape, oct, sem;
                float pw, sub, lvl, fin, crs;
                if(!(ss >> ti >> u >> en >> shape >> pw >> sub >> lvl >> oct >> sem >> fin >> crs))
                    continue;
                ensureTrack(ti);
                if(ti < 0 || u < 0 || u >= synth::kBasicOscUnits) continue;
                auto &bu = tracks[(size_t)ti].basicUnits[(size_t)u];
                bu.enabled = en != 0;
                bu.shape = synth::BasicOscillatorShape(clampi(shape, 0, 4));
                bu.pulseWidth = clampf(pw, 0.05f, 0.95f);
                bu.subLevel = clampf(sub, 0.0f, 1.0f);
                bu.level = clampf(lvl, 0.0f, 1.0f);
                bu.pitchOct = clampi(oct, -4, 4);
                bu.pitchSem = clampi(sem, -12, 12);
                bu.pitchFin = clampf(fin, -100.0f, 100.0f);
                bu.pitchCrs = clampf(crs, -100.0f, 100.0f);
            }
            else if(tok == "mmod")
            {
                int ti, s, src, type; float depth;
                if(!(ss >> ti >> s >> src >> type >> depth))
                    continue;
                int kind = 0, node = 0;
                if(!(ss >> kind)) kind = 0;
                if(!(ss >> node)) node = 0;
                ensureTrack(ti);
                if(ti < 0 || s < 0 || s >= synth::kMaxTrackMods) continue;
                auto &m = tracks[(size_t)ti].mods[(size_t)s];
                m.enabled = true;
                m.sourceTrack = int8_t(src);
                m.type = synth::SourceModType(type);
                m.depth = depth;
                m.sourceKind = uint8_t(kind);
                m.sourceNode = uint8_t(node);
            }
            else if(tok == "mslot")
            {
                int i, en, loop, pc; float rate;
                if(!(ss >> i >> en >> loop >> rate >> pc)) continue;
                if(i < 0 || i >= synth::kMaxModSlots) continue;
                auto &m = parsedSlots[(size_t)i];
                m.enabled = en != 0;
                m.loop = loop != 0;
                m.rateHz = clampf(rate, 0.0f, 100.0f);
                m.pointCount = clampi(pc, 2, synth::kMaxMatrixEnvPoints);
                // Tail fields, appended after the format shipped. A failed
                // extraction writes 0 into its target, so each is re-defaulted
                // individually rather than trusting one combined read.
                int sync = 0, div = synth::kSyncDivDefault;
                if(!(ss >> sync)) sync = 0;
                if(!(ss >> div)) div = synth::kSyncDivDefault;
                m.tempoSync = sync != 0;
                m.syncDiv = uint8_t(synth::clampSyncDiv(div));
                hasSlots = true;
            }
            else if(tok == "mtempo")
            {
                float bpm;
                if(!(ss >> bpm)) continue;
                uiTempoBpm_ = clampf(bpm, 20.0f, 300.0f);
                if(auto *pl = plugin()) pl->setUiTempoBpm(uiTempoBpm_);
            }
            else if(tok == "mslotp")
            {
                int i, k; float px, py, pc;
                if(!(ss >> i >> k >> px >> py >> pc)) continue;
                if(i < 0 || i >= synth::kMaxModSlots
                   || k < 0 || k >= synth::kMaxMatrixEnvPoints) continue;
                auto &pt = parsedSlots[(size_t)i].points[(size_t)k];
                pt.x = clampf(px, 0.0f, 1.0f);
                pt.y = clampf(py, 0.0f, 1.0f);
                pt.curve = clampf(pc, -1.0f, 1.0f);
                hasSlots = true;
            }
            else if(tok == "mchaos")
            {
                int en, type; float f, amt;
                if(!(ss >> en >> type >> f >> amt)) continue;
                parsedChaos.enabled = en != 0;
                parsedChaos.type = synth::ChaosNoiseType(clampi(type, 0, 2));
                parsedChaos.frequencyHz = clampf(f, 0.01f, 100.0f);
                parsedChaos.amount = clampf(amt, 0.0f, 1.0f);
                hasSlots = true;
            }
            else if(tok == "mshape")
            {
                int shp, spec; float ph, rho, up, dn;
                if(!(ss >> shp >> ph >> rho >> up >> dn >> spec)) continue;
                parsedShape.shape = synth::LfoShape(clampi(shp, 0, 4));
                parsedShape.phase0 = clampf(ph, 0.0f, 1.0f);
                parsedShape.rho = clampf(rho, 0.0f, 1.0f);
                parsedShape.pUp = clampf(up, 0.1f, 8.0f);
                parsedShape.pDown = clampf(dn, 0.1f, 8.0f);
                parsedShape.useSpectralX = spec != 0;
                hasSlots = true;
            }
            else if(tok == "mrules")
            {
                // Rules-section marker: even with zero mrule/mgrp lines, restore
                // (i.e. clear) the tables instead of preserving the previous
                // preset's.
                hasRules = true;
            }
            else if(tok == "mgrp")
            {
                int gi, base, family, fdest; unsigned ftid; float fs, ps, sc, fdepth;
                if(!(ss >> gi >> base >> fs >> ps >> sc >> family >> fdest >> ftid >> fdepth))
                    continue;
                // Optional tail fields, newest last. Each needs its own re-default:
                // once one >> fails the stream stays in fail state AND writes 0.
                int en = 1, wsrc = 0;
                unsigned wtid = 0;
                float rate = 1.0f;
                if(!(ss >> en)) en = 1;      // lines written before the field existed
                if(!(ss >> wsrc)) wsrc = 0;
                if(!(ss >> wtid)) wtid = 0;
                if(!(ss >> rate)) rate = 1.0f;
                if(gi < 0 || gi >= synth::kMaxMaskGroups) continue;
                auto &g = parsedGroups[(size_t)gi];
                g.enabled = (en != 0);
                g.waveSource = uint8_t(wsrc != 0);
                g.waveTrackId = wtid;
                g.rateHz = clampf(rate, 0.01f, 40.0f);
                g.baseSlot = int8_t(clampi(base, 0, synth::kMaxModSlots - 1));
                g.freqSpread = clampf(fs, -1.0f, 1.0f);
                g.phaseSpread = clampf(ps, -1.0f, 1.0f);
                g.spreadCurve = clampf(sc, -1.0f, 1.0f);
                g.family = uint8_t(family != 0);
                g.familyDest = synth::ModDestination(fdest);
                g.familyTrackId = ftid;
                g.familyDepth = fdepth;
                hasRules = true;
            }
            else if(tok == "mgt")
            {
                int gi, k, dest; unsigned tid; float depth;
                if(!(ss >> gi >> k >> dest >> tid >> depth))
                    continue;
                if(gi < 0 || gi >= synth::kMaxMaskGroups
                   || k < 0 || k >= synth::kMaskGroupSlots) continue;
                auto &t = parsedGroups[(size_t)gi].targets[(size_t)k];
                t.enabled = true;
                t.dest = synth::ModDestination(dest);
                t.targetTrackId = tid;
                t.depth = depth;
                hasRules = true;
            }
            else if(tok == "mrule")
            {
                // NOTE: since C++11 a failed >> extraction WRITES 0 into the
                // target — optional tail fields must be re-defaulted explicitly,
                // never just pre-initialized.
                int ri, src, dst, weight, lo, hi, slot;
                unsigned tid; float depth;
                if(!(ss >> ri >> src >> dst >> depth >> weight >> lo >> hi >> tid >> slot))
                    continue;
                int mask = -1, axis = 0, mutedIn = 0;
                float xfer = 0.0f;
                if(!(ss >> mask)) mask = -1;
                if(!(ss >> axis)) axis = 0;
                if(!(ss >> xfer)) xfer = 0.0f;
                if(!(ss >> mutedIn)) mutedIn = 0;
                if(ri < 0 || ri >= synth::kMaxMatrixRules) continue;
                auto &ru = parsedRules[(size_t)ri];
                ru.enabled = true;
                ru.source = synth::ModSource(src);
                ru.dest = synth::ModDestination(dst);
                ru.depth = depth;
                ru.targetTrackId = tid;
                ru.targetSlot = slot;
                // Legacy per-rule filtering (weight/band/mask) is superseded by
                // MASK GROUPS and has no UI in the basic tier — the fields are
                // parsed for format compatibility but left at their neutral
                // defaults so a loaded preset never applies invisible shaping.
                (void)weight; (void)lo; (void)hi; (void)mask; (void)axis;
                ru.transferCurve = clampf(xfer, -1.0f, 1.0f);
                ru.muted = uint8_t(mutedIn != 0);
                hasRules = true;
            }
            else if(tok == "mins")
            {
                int ti, ii, kind, byp; ss >> ti >> ii >> kind >> byp; ensureTrack(ti);
                if(ti < 0 || ii < 0) continue;
                InsertEffect e; e.kind = uint8_t(kind); e.bypass = byp != 0;
                std::string hf, hd, he, hc, hdl, hr; ss >> hf >> hd >> he >> hc >> hdl >> hr;
                unhexPod(hf, e.filter); unhexPod(hd, e.dist); unhexPod(he, e.eq);
                unhexPod(hc, e.comp); unhexPod(hdl, e.delay); unhexPod(hr, e.reverb);
                auto &v = tracks[(size_t)ti].inserts;
                if(int(v.size()) <= ii) v.resize((size_t)ii + 1);
                v[(size_t)ii] = e;
            }
            else if(tok == "minsconv")
            {
                int ti, ii; ss >> ti >> ii; if(ti < 0 || ii < 0 || ti >= int(tracks.size())) continue;
                auto &v = tracks[(size_t)ti].inserts;
                if(ii >= int(v.size())) continue;
                std::string name; std::getline(ss, name);
                if(!name.empty() && name.front() == ' ') name.erase(name.begin());
                v[(size_t)ii].conv.irName = name;
            }
            else if(tok == "mwire")
            {
                synth::GridWire w; unsigned fn, tn; int fp, tp; ss >> fn >> fp >> tn >> tp;
                w.from = { fn, uint8_t(fp) }; w.to = { tn, uint8_t(tp) }; wires.push_back(w);
            }
            else if(tok == "moutp") { unsigned id; int c; ss >> id >> c; outPorts[id] = c; }
            else if(tok == "mutc") { unsigned id; int c; ss >> id >> c; utilCount[id] = c; }
            else if(tok == "mutp")
            {
                uint64_t k; synth::RouteUtilParams up; int on;
                ss >> k >> up.level >> up.pan >> up.bandLoHz >> up.bandHiHz >> on; up.bandOn = on; utilParams[k] = up;
            }
            else if(tok == "mnp") { unsigned c; int local, x, y; ss >> c >> local >> x >> y; nodePos[c][local] = { x, y }; }
            else if(tok == "msw")
            {
                unsigned c, fn, tn; int fp, tp; ss >> c >> fn >> fp >> tn >> tp;
                synth::GridWire w; w.from = { fn, uint8_t(fp) }; w.to = { tn, uint8_t(tp) };
                sWires[c].push_back(w);
            }
            else if(tok == "mmeta")
            {
                int ti; ss >> ti; if(ti < 0) continue; ensureTrack(ti);
                auto &m = tracks[(size_t)ti].metaOsc;
                int en = 1, warp = 0;
                ss >> en >> m.ratio >> m.amp >> m.phase >> m.phaseRandom >> m.pan
                   >> m.frameCount >> m.morph >> warp >> m.warpAmount
                   >> m.pitchOct >> m.pitchSem >> m.pitchFin >> m.pitchCrs;
                m.enabled = en != 0;
                m.warpMode = synth::WavetableWarpMode(warp);
                markOsc(ti);
            }
            else if(tok == "mmetaf")
            {
                int ti, f, bins; ss >> ti >> f >> bins; if(ti < 0) continue; ensureTrack(ti);
                std::string payload; ss >> payload;
                decodeFrameBins(tracks[(size_t)ti].metaOsc.frames, f, bins, payload);
                markOsc(ti);
            }
            else if(tok == "mpb")
            {
                int ti; ss >> ti; if(ti < 0) continue; ensureTrack(ti);
                auto &b = tracks[(size_t)ti].partialBank;
                int shape = 0;
                ss >> b.partialCount >> shape >> b.inharmonicAmount >> b.frameCount >> b.morph;
                b.freqShape = synth::FreqShape(shape);
                markOsc(ti);
            }
            else if(tok == "mpbf")
            {
                int ti, f, bins; ss >> ti >> f >> bins; if(ti < 0) continue; ensureTrack(ti);
                std::string payload; ss >> payload;
                decodeFrameBins(tracks[(size_t)ti].partialBank.frames, f, bins, payload);
                markOsc(ti);
            }
            else if(tok == "mpbp")
            {
                int ti, i; ss >> ti >> i; if(ti < 0 || i < 0 || i >= synth::kMaxWavetablePartials) continue;
                ensureTrack(ti);
                auto &p = tracks[(size_t)ti].partialBank.partials[(size_t)i];
                int en = 1, warp = 0;
                ss >> en >> p.ratio >> p.amp >> p.phase >> p.phaseRandom >> p.pan
                   >> p.frameCount >> p.morph >> warp >> p.warpAmount
                   >> p.pitchOct >> p.pitchSem >> p.pitchFin >> p.pitchCrs;
                p.enabled = en != 0;
                p.warpMode = synth::WavetableWarpMode(warp);
                markOsc(ti);
            }
            else if(tok == "mpbpf")
            {
                int ti, i, f, bins; ss >> ti >> i >> f >> bins;
                if(ti < 0 || i < 0 || i >= synth::kMaxWavetablePartials) continue;
                ensureTrack(ti);
                std::string payload; ss >> payload;
                decodeFrameBins(tracks[(size_t)ti].partialBank.partials[(size_t)i].frames, f, bins, payload);
                markOsc(ti);
            }
            else if(tok == "mrnp") { unsigned id; int x, y; ss >> id >> x >> y; boardPos[id] = { x, y }; }
            else if(tok == "msgn")
            {
                int gi; ss >> gi; if(gi < 0) continue;
                if(gi >= int(groups.size())) groups.resize((size_t)gi + 1);
                std::string name; std::getline(ss, name);
                if(!name.empty() && name.front() == ' ') name.erase(name.begin());
                groups[(size_t)gi].name = name;
            }
            else if(tok == "msgm")
            {
                int gi, mi; ss >> gi >> mi; if(gi < 0 || mi < 0) continue;
                if(gi >= int(groups.size())) groups.resize((size_t)gi + 1);
                groups[(size_t)gi].memberIndices.push_back(mi);
            }
            else if(tok == "maes")
            {
                int i, slot; ss >> i >> slot;
                if(i >= 0 && i < synth::kMaxAmpEnvRouteNodes && slot >= 0 && slot < 256)
                    envSlots[(size_t)i] = uint8_t(slot);
            }
        }
        // No modern section => this patch has no router. Do NOT fall through
        // keeping the previous one's graph; rebuild a coherent default instead.
        if(!hasModern)
        {
            resetRouterGraphToDefaultChains();
            return;
        }
        // Oscillator content is carried across for any track the file did not
        // describe. Every preset written before the osc tokens existed lands
        // here: the parsed tracks are default-constructed apart from the scalars
        // on their `mtrack` line, so assigning them straight over would silently
        // destroy the Partial Bank and Meta wavetables the patch is playing —
        // the load wiping exactly what it was asked to restore. Match by track
        // id, not index, so a reordered rack still finds its own tables.
        if(!tracks.empty())
        {
            for(size_t i = 0; i < tracks.size(); ++i)
            {
                if(std::find(oscSeen.begin(), oscSeen.end(), int(i)) != oscSeen.end())
                    continue;
                for(const auto &prev : generator_.tracks)
                {
                    if(prev.id != tracks[i].id)
                        continue;
                    // `mtrack` carries partialCount even in an old preset, so it
                    // stays authoritative; only the tables come from memory.
                    const int pc = tracks[i].partialBank.partialCount;
                    tracks[i].partialBank = prev.partialBank;
                    tracks[i].partialBank.partialCount = pc;
                    tracks[i].metaOsc = prev.metaOsc;
                    break;
                }
            }
            generator_.tracks = tracks;
        }
        // Sample audio is not embedded in the preset; re-read each referenced
        // file now that the track list is in place.
        for(auto &t : generator_.tracks)
            if(t.type == synth::SourceTrackType::SampleNoise && !t.sampler.samplePath.empty()
               && !t.sampler.sample)
            {
                const auto keep = t.sampler;   // loading resets the window
                if(loadSampleIntoTrack(t, keep.samplePath))
                {
                    t.sampler.rootNote = keep.rootNote;
                    t.sampler.keyTrack = keep.keyTrack;
                    t.sampler.loopMode = keep.loopMode;
                    t.sampler.startNorm = keep.startNorm;
                    t.sampler.endNorm = keep.endNorm;
                    t.sampler.loopStartNorm = keep.loopStartNorm;
                    t.sampler.loopEndNorm = keep.loopEndNorm;
                    t.sampler.sliceCount = keep.sliceCount;
                    t.sampler.gain = keep.gain;
                    t.sampler.reverse = keep.reverse;
                }
            }
        routeWires_ = wires;
        nodeOutPortCount_ = outPorts;
        structUtilCount_ = utilCount;
        structUtilParams_ = utilParams;
        structNodePos_ = nodePos;
        structWires_ = sWires;
        // Replaced wholesale rather than merged. Leaving the previous patch's
        // entries in place made try_emplace a no-op for every node id that
        // happened to collide, so a freshly loaded rack came up wearing the old
        // one's coordinates while the rest fell to defaults. An absent mrnp
        // section now means "lay it out fresh", not "keep whatever was there".
        routeNodePositions_ = boardPos;
        ampEnvRouteNodeSlots_ = envSlots;
        // Members are track INDICES, so they only mean anything against the track
        // list restored just above: drop stale ones, then the groups they empty
        // out (two members minimum, the same invariant the delete path keeps).
        for(auto &g : groups)
        {
            std::vector<int> kept;
            for(const int mi : g.memberIndices)
                if(mi >= 0 && mi < int(generator_.tracks.size()))
                    kept.push_back(mi);
            g.memberIndices = std::move(kept);
        }
        groups.erase(std::remove_if(groups.begin(), groups.end(),
                                    [](const StripGroup &g) { return g.memberIndices.size() < 2; }),
                     groups.end());
        stripGroups_ = std::move(groups);
        if(selectedGroupView_ >= int(stripGroups_.size()))
            selectedGroupView_ = -1;
        pushGroups();
        selectedTrack_ = clampi(selectedTrack_, 0, std::max(0, int(generator_.tracks.size()) - 1));
        // Rules restored before the rebuild so any strip-insert adoption during the
        // rebuild remaps them consistently with the wires.
        if(hasRules)
        {
            rules_ = parsedRules;
            maskGroups_ = parsedGroups;
        }
        if(hasSlots)
        {
            modSlots_ = parsedSlots;
            chaos_ = parsedChaos;
            shape_ = parsedShape;
            if(auto *p = plugin())
            {
                for(int i = 0; i < synth::kMaxModSlots; ++i)
                    p->updateModSlot(i, modSlots_[(size_t)i]);
                p->updateChaos(chaos_);
                p->updateShapeSource(shape_);
            }
        }
        // Recomputes each track's routing fields (filter/insert order, connectedToMaster)
        // from the restored wires, then pushes all tracks + the compiled route to the engine.
        rebuildSelectedPerVoiceRouteFromWires();
        if(hasRules)
            if(auto *p = plugin())
            {
                for(int i = 0; i < synth::kMaxMatrixRules; ++i)
                    p->updateMatrixRule(i, rules_[(size_t)i]);
                for(int i = 0; i < synth::kMaxMaskGroups; ++i)
                    p->updateMaskGroup(i, maskGroups_[(size_t)i]);
            }
    }

void KapibaraUI::appendPresetNameChar(char ch)
{
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')
                        || ch == '_' || ch == '-' || ch == ' ';
        if(!ok || presetNameBuffer_.size() >= 64)
            return;
        presetNameBuffer_.push_back(ch);
    }

std::string KapibaraUI::trimPresetName(std::string s)
{
        while(!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
            s.erase(s.begin());
        while(!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
            s.pop_back();
        return s;
    }

std::string KapibaraUI::safeFileStem(std::string s, const char *fallback)
{
        s = trimPresetName(std::move(s));
        std::string out;
        for(char ch : s)
        {
            const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')
                            || ch == '_' || ch == '-' || ch == ' ';
            if(ok)
                out.push_back(ch);
        }
        return out.empty() ? std::string(fallback) : out;
    }

void KapibaraUI::beginSynthPresetRename()
{
        if(selectedPresetIndex_ >= 0 && selectedPresetIndex_ < int(presetNames_.size()))
            presetNameBuffer_ = presetNames_[(size_t)selectedPresetIndex_];
        else if(presetNameBuffer_.empty())
            presetNameBuffer_ = "user_kapibara";
        presetNameEditing_ = true;
        presetNameEditTarget_ = PresetNameEditTarget::Synth;
        skipNextPresetCharacterInput_ = false;
    }

void KapibaraUI::beginWavetablePresetRename()
{
        if(selectedWavetablePresetIndex_ >= 0 && selectedWavetablePresetIndex_ < int(wavetablePresets_.size()))
            presetNameBuffer_ = wavetablePresets_[(size_t)selectedWavetablePresetIndex_].name;
        else if(!wavetablePresetLabel_.empty() && wavetablePresetLabel_ != "Select Wavetable")
            presetNameBuffer_ = wavetablePresetLabel_;
        else
            presetNameBuffer_ = "wavetable";
        presetNameEditing_ = true;
        presetNameEditTarget_ = PresetNameEditTarget::Wavetable;
        skipNextPresetCharacterInput_ = false;
    }

void KapibaraUI::commitPresetNameEdit()
{
        const auto target = presetNameEditTarget_;
        presetNameEditing_ = false;
        presetNameEditTarget_ = PresetNameEditTarget::None;
        skipNextPresetCharacterInput_ = false;
        const std::string clean = safeFileStem(presetNameBuffer_, target == PresetNameEditTarget::Wavetable ? "wavetable" : "user_kapibara");
        presetNameBuffer_ = clean;
        if(target == PresetNameEditTarget::Synth)
        {
            releaseAllUiNotes();
            if(auto *p = plugin())
            {
                p->saveUserPreset(clean.c_str());
                saveModernState(p->presetFilePath(clean.c_str()));
            }
            pullFromPlugin();
            for(int i = 0; i < int(presetNames_.size()); ++i)
                if(presetNames_[(size_t)i] == clean)
                    selectedPresetIndex_ = i;
            presetLabel_ = clean;
        }
        else if(target == PresetNameEditTarget::Wavetable)
        {
            saveOrRenameWavetablePreset(clean);
        }
        else if(target == PresetNameEditTarget::Router)
        {
            saveRouterPreset(clean);
        }
    }


// Host session state. The engine half comes from the plugin; this pushes the UI
// half (tracks, wires, matrix, mask groups, MOD slots) so getState can hand the
// DAW a complete patch. Throttled: it serializes the whole patch, and every
// knob drag would otherwise do it per motion event.
void KapibaraUI::pushHostState(bool force)
{
        auto *p = plugin();
        if(p == nullptr)
            return;
        const uint64_t now = uiNowMs();
        if(!force && now - lastHostStateMs_ < 400u)
        {
            hostStateDirty_ = true;
            return;
        }
        lastHostStateMs_ = now;
        hostStateDirty_ = false;
        setState("patch", modernStateString().c_str());
    }

// The host restored a session (or another instance's state). The plugin has
// already applied the engine half in its own setState; this picks up the UI half
// out of the same blob.
void KapibaraUI::stateChanged(const char *key, const char *value)
{
        if(key == nullptr || value == nullptr || std::strcmp(key, "patch") != 0)
            return;
        pullFromPlugin();
        std::istringstream in(value);
        readModernState(in);
        repaint();
    }

END_NAMESPACE_DISTRHO

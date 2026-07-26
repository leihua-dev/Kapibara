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
} // namespace

// Modern-state persistence: the legacy preset (plugin side) doesn't save the
// multi-track routing/structure, so the UI appends it here and parses it back.
void KapibaraUI::saveModernState(const std::string &path)
{
        std::ofstream out(path, std::ios::app);
        if(!out) return;
        out << "modern 1\n";
        for(size_t ti = 0; ti < generator_.tracks.size(); ++ti)
        {
            const auto &t = generator_.tracks[ti];
            out << "mtrack " << ti << ' ' << t.id << ' ' << int(t.type) << ' ' << t.gain << ' ' << t.pan << ' ' << t.send
                << ' ' << t.ampEnvIndex << ' ' << int(t.mute) << ' ' << int(t.solo)
                << ' ' << t.unison.voices << ' ' << t.unison.detuneCents << ' ' << t.unison.widthStereo << ' ' << t.unison.phaseSpread
                << ' ' << int(t.basicShape) << ' ' << t.pulseWidth << ' ' << t.subLevel << ' ' << int(t.sampleNoiseMode) << ' ' << t.noiseColor
                << ' ' << t.partialBank.partialCount << ' ' << t.perVoiceFilterCount << ' ' << t.inserts.size() << "\n";
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
            bool hasContent = g.enabled || g.family != 0;
            for(const auto &t : g.targets)
                if(t.enabled) { hasContent = true; break; }
            if(!hasContent) continue;
            out << "mgrp " << gi << ' ' << int(g.baseSlot) << ' ' << g.freqSpread << ' '
                << g.phaseSpread << ' ' << g.spreadCurve << ' ' << int(g.family) << ' '
                << int(g.familyDest) << ' ' << g.familyTrackId << ' ' << g.familyDepth << ' '
                << int(g.enabled) << "\n";
            for(int k = 0; k < synth::kMaskGroupSlots; ++k)
            {
                const auto &t = g.targets[(size_t)k];
                if(!t.enabled) continue;
                out << "mgt " << gi << ' ' << k << ' ' << int(t.dest) << ' '
                    << t.targetTrackId << ' ' << t.depth << "\n";
            }
        }
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
    }

void KapibaraUI::loadModernState(const std::string &path)
{
        std::ifstream in(path);
        if(!in) return;
        bool hasModern = false;
        std::vector<synth::SourceTrackParams> tracks;
        std::vector<synth::GridWire> wires;
        std::unordered_map<uint32_t, int> outPorts;
        std::unordered_map<uint32_t, int> utilCount;
        std::unordered_map<uint64_t, synth::RouteUtilParams> utilParams;
        std::unordered_map<uint32_t, std::unordered_map<int, synth::GridPoint>> nodePos;
        std::unordered_map<uint32_t, std::vector<synth::GridWire>> sWires;
        std::array<synth::MatrixRule, synth::kMaxMatrixRules> parsedRules {};
        std::array<synth::MaskGroup, synth::kMaxMaskGroups> parsedGroups {};
        bool hasRules = false;
        const auto ensureTrack = [&](int ti) { if(ti >= 0 && ti >= int(tracks.size())) tracks.resize((size_t)ti + 1); };
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
                   >> bshape >> t.pulseWidth >> t.subLevel >> snmode >> t.noiseColor >> pc >> pvfc >> insc;
                t.id = id; t.type = synth::SourceTrackType(type); t.mute = mute; t.solo = solo;
                t.basicShape = synth::BasicOscillatorShape(bshape); t.sampleNoiseMode = synth::SampleNoiseMode(snmode);
                t.partialBank.partialCount = pc; t.perVoiceFilterCount = pvfc; t.inserts.clear();
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
                int en = 1;
                if(!(ss >> en)) en = 1;  // lines written before the field existed
                if(gi < 0 || gi >= synth::kMaxMaskGroups) continue;
                auto &g = parsedGroups[(size_t)gi];
                g.enabled = (en != 0);
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
        }
        if(!hasModern) return;
        if(!tracks.empty()) generator_.tracks = tracks;
        routeWires_ = wires;
        nodeOutPortCount_ = outPorts;
        structUtilCount_ = utilCount;
        structUtilParams_ = utilParams;
        structNodePos_ = nodePos;
        structWires_ = sWires;
        selectedTrack_ = clampi(selectedTrack_, 0, std::max(0, int(generator_.tracks.size()) - 1));
        // Rules restored before the rebuild so any strip-insert adoption during the
        // rebuild remaps them consistently with the wires.
        if(hasRules)
        {
            rules_ = parsedRules;
            maskGroups_ = parsedGroups;
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
    }


END_NAMESPACE_DISTRHO

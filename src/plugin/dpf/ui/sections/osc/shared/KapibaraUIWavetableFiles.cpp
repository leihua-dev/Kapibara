#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// ---- IR convolution reverb: impulse files live in presets/irs (.wav) ----


    // Save the current Meta Oscillator's harmonic wavetable straight into the
    // presets/wavetables folder (no OS file dialog — those are unreliable under
    // some Wayland compositors). The saved .kwt then appears in the preset list.


    // Serializes the current Meta Oscillator's per-frame harmonics (ratio/amp/phase)
    // to a plain-text .kwt file. Not a WAV: only the additive spectrum is stored.

    // Loads a .kwt harmonic/phase wavetable into the current Meta Oscillator or
    // PartialBank. PartialBank uses only harmonics[0..63] as frame amp/phase.
void KapibaraUI::beginWavetableImport()
{
        droppedWavPending_ = false;
        wavetablePresetMenuOpen_ = false;
        wavetableImportMenuOpen_ = true;
    }

void KapibaraUI::openWavetableFileBrowser()
{
#if DISTRHO_UI_FILE_BROWSER
        FileBrowserOptions options;
        options.title = "Import wavetable WAV";
        if(!lastLoadPath_.empty())
        {
            const size_t slash = lastLoadPath_.find_last_of("/\\");
            browserStartDir_ = slash == std::string::npos ? std::string {} : lastLoadPath_.substr(0, slash);
            if(!browserStartDir_.empty())
                options.startDir = browserStartDir_.c_str();
        }
        if(openFileBrowser(options))
            return;
#endif
        loadPathEditing_ = true;
        if(loadPathBuffer_.empty())
            loadPathBuffer_ = lastLoadPath_;
    }

bool KapibaraUI::commitWavetableLoad()
{
        loadPathEditing_ = false;
        if(loadPathBuffer_.empty())
            return false;

        lastLoadPath_ = loadPathBuffer_;
        bool ok = false;
        // Kapibara native harmonic/phase wavetable round-trip.
        if(loadPathBuffer_.size() >= 4
           && loadPathBuffer_.compare(loadPathBuffer_.size() - 4, 4, ".kwt") == 0)
        {
            ok = loadWavetableHarmonicFile(loadPathBuffer_);
            if(ok)
                pullFromPlugin();
            return ok;
        }
        if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
        {
            pushMetaUndoSnapshot();
            synth::WavetableImportOptions options;
            options.mode = wavetableImportMode_;
            options.frameLength = synth::kWavetableSize;
            options.manualCycleLength = manualCycleLength_;
            options.maxFrames = importFrameLimit_;
            const auto result = synth::importWavetableFramesFromWav(loadPathBuffer_, track->metaOsc, 0, options);
            ok = result.success;
            if(ok)
            {
                selectedMetaFrame_ = 0;
                metaFrameSelected_.fill(false);
                metaFrameSelected_[0] = true;
                metaFrameRangeAnchor_ = 0;
                pushCurrentTrack();
                loadStatus_ = result.message;
            }
            else
                loadStatus_ = "load failed: " + result.message;
        }
        else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
        {
            (void)track;
            loadStatus_ = "PartialBank loads .kwt harmonic tables";
            ok = false;
        }
        else if(auto *p = plugin())
        {
            ok = p->loadWavetableFrame(selectedMetaPartial_, selectedMetaFrame_, loadPathBuffer_.c_str());
        }
        if(currentTrack() == nullptr || currentTrack()->type != synth::SourceTrackType::MetaOscillator)
            loadStatus_ = ok ? ("loaded " + loadPathBuffer_) : ("load failed: " + loadPathBuffer_);
        if(ok)
        {
            const size_t slash = loadPathBuffer_.find_last_of("/\\");
            const size_t nameStart = slash == std::string::npos ? 0 : slash + 1;
            const size_t dot = loadPathBuffer_.find_last_of('.');
            wavetablePresetLabel_ = loadPathBuffer_.substr(
                nameStart, dot == std::string::npos || dot < nameStart ? std::string::npos : dot - nameStart);
            pullFromPlugin();
            auto *track = currentTrack();
            auto &slot = (track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                             ? track->metaOsc
                             : generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
            selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, slot.frameCount - 1));
        }
        return ok;
    }

void KapibaraUI::refreshIrFiles()
{
        irFiles_.clear();
        std::error_code ec;
        const std::filesystem::path dir("presets/irs");
        std::filesystem::create_directories(dir, ec);
        for(const auto &entry : std::filesystem::directory_iterator(dir, ec))
        {
            if(ec) break;
            if(!entry.is_regular_file(ec)) continue;
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return char(std::tolower(c)); });
            if(ext != ".wav") continue;
            irFiles_.push_back({ entry.path().stem().string(), entry.path().string() });
        }
        std::sort(irFiles_.begin(), irFiles_.end(),
                  [](const auto &a, const auto &b){ return a.first < b.first; });
    }

void KapibaraUI::loadImpulseIntoInsert(InsertEffect &e, const std::string &name, const std::string &path)
{
        std::vector<float> samples;
        uint32_t sr = 48000;
        if(!synth::loadImpulseResponseMono(path, samples, sr))
        {
            metaEditorStatus_ = "IR load failed: " + name;
            return;
        }
        const double engineSr = getSampleRate() > 1000.0 ? getSampleRate() : 48000.0;
        // hop=512 → ~11ms latency; cap IR at 3 seconds.
        e.conv.ir = synth::fx::buildConvIR(samples, 512, 3.0f, engineSr, name);
        e.conv.irName = name;
        metaEditorStatus_ = "IR loaded: " + name;
    }

void KapibaraUI::openWavetableSaveBrowser()
{
        auto *track = currentTrack();
        if(track == nullptr
           || (track->type != synth::SourceTrackType::MetaOscillator
               && track->type != synth::SourceTrackType::PartialBank))
        {
            metaEditorStatus_ = "select Meta or PartialBank to save";
            return;
        }
        std::string dir = "presets/wavetables";
        if(auto *p = plugin())
            dir = p->wavetableUserDir();
        std::string base = wavetablePresetLabel_.empty() ? std::string("wavetable") : wavetablePresetLabel_;
        // Strip any directory part the label may carry.
        const size_t slash = base.find_last_of("/\\");
        if(slash != std::string::npos)
            base = base.substr(slash + 1);
        // Pick a unique filename so saves don't silently overwrite.
        std::error_code ec;
        std::string path = dir + "/" + base + ".kwt";
        int suffix = 2;
        while(std::filesystem::exists(path, ec))
            path = dir + "/" + base + "_" + std::to_string(suffix++) + ".kwt";
        saveWavetableToFile(path);
        refreshWavetablePresets();
    }

void KapibaraUI::saveOrRenameWavetablePreset(const std::string &newStem)
{
        std::string dir = "presets/wavetables";
        if(auto *p = plugin())
            dir = p->wavetableUserDir();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        const bool hasSelection = selectedWavetablePresetIndex_ >= 0
                                  && selectedWavetablePresetIndex_ < int(wavetablePresets_.size());
        if(hasSelection)
        {
            const auto oldPath = std::filesystem::path(wavetablePresets_[(size_t)selectedWavetablePresetIndex_].path);
            const auto ext = oldPath.extension().empty() ? std::filesystem::path(".kwt") : oldPath.extension();
            auto newPath = oldPath.parent_path() / (newStem + ext.string());
            if(newPath != oldPath)
            {
                int suffix = 2;
                while(std::filesystem::exists(newPath, ec))
                    newPath = oldPath.parent_path() / (newStem + "_" + std::to_string(suffix++) + ext.string());
                std::filesystem::rename(oldPath, newPath, ec);
                if(ec)
                {
                    metaEditorStatus_ = "rename failed: " + oldPath.string();
                    loadStatus_ = metaEditorStatus_;
                    return;
                }
                lastLoadPath_ = newPath.string();
            }
            wavetablePresetLabel_ = newStem;
            metaEditorStatus_ = "renamed wavetable: " + newStem;
            loadStatus_ = metaEditorStatus_;
            refreshWavetablePresets();
            for(int i = 0; i < int(wavetablePresets_.size()); ++i)
                if(wavetablePresets_[(size_t)i].name == newStem)
                    selectedWavetablePresetIndex_ = i;
            return;
        }

        std::filesystem::path path = std::filesystem::path(dir) / (newStem + ".kwt");
        int suffix = 2;
        while(std::filesystem::exists(path, ec))
            path = std::filesystem::path(dir) / (newStem + "_" + std::to_string(suffix++) + ".kwt");
        saveWavetableToFile(path.string());
        refreshWavetablePresets();
        for(int i = 0; i < int(wavetablePresets_.size()); ++i)
            if(wavetablePresets_[(size_t)i].path == path.string())
                selectedWavetablePresetIndex_ = i;
    }

void KapibaraUI::saveWavetableToFile(const std::string &path)
{
        auto *track = currentTrack();
        if(track == nullptr
           || (track->type != synth::SourceTrackType::MetaOscillator
               && track->type != synth::SourceTrackType::PartialBank))
        {
            metaEditorStatus_ = "select Meta or PartialBank to save";
            return;
        }
        const bool isBank = track->type == synth::SourceTrackType::PartialBank;
        const int frameCount = isBank
                                   ? clampi(track->partialBank.frameCount, 1, synth::kMaxWavetableFrames)
                                   : clampi(track->metaOsc.frameCount, 1, synth::kMaxWavetableFrames);
        const auto &frameStorage = isBank ? track->partialBank.frames : track->metaOsc.frames;
        const int harmonicLimit = isBank ? synth::kMaxWavetablePartials : synth::kMaxWavetableHarmonics;
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if(!out)
        {
            metaEditorStatus_ = "save failed: " + path;
            loadStatus_ = metaEditorStatus_;
            return;
        }
        Kwt2Header header;
        header.frameCount = uint32_t(frameCount);
        header.binCount = uint32_t(harmonicLimit);
        out.write(reinterpret_cast<const char *>(&header), sizeof(header));
        const auto &frames = frameStorage.get();
        for(int f = 0; f < frameCount; ++f)
        {
            const auto &fp = frames[(size_t)f];
            const synth::WavetableFrame *frame = fp ? fp.get() : nullptr;
            for(int h = 0; h < harmonicLimit; ++h)
            {
                const auto &hm = frame != nullptr ? frame->harmonics[(size_t)h] : synth::WavetableHarmonic {};
                const auto packed = packKwtBin(hm.amp, hm.phase);
                out.write(reinterpret_cast<const char *>(&packed), sizeof(packed));
            }
        }
        out.close();
        const size_t slash = path.find_last_of("/\\");
        lastLoadPath_ = path;
        wavetablePresetLabel_ = path.substr(slash == std::string::npos ? 0 : slash + 1);
        const size_t dot = wavetablePresetLabel_.find_last_of('.');
        if(dot != std::string::npos)
            wavetablePresetLabel_ = wavetablePresetLabel_.substr(0, dot);
        metaEditorStatus_ = "saved " + path;
        loadStatus_ = metaEditorStatus_;
    }

bool KapibaraUI::loadWavetableHarmonicFile(const std::string &path)
{
        auto *track = currentTrack();
        if(track == nullptr
           || (track->type != synth::SourceTrackType::MetaOscillator
               && track->type != synth::SourceTrackType::PartialBank))
            return false;
        const bool isBank = track->type == synth::SourceTrackType::PartialBank;
        {
            std::ifstream bin(path, std::ios::binary);
            Kwt2Header header;
            if(bin.read(reinterpret_cast<char *>(&header), sizeof(header))
               && std::memcmp(header.magic, "KWT2", 4) == 0)
            {
                const int frameCount = clampi(int(header.frameCount), 1, synth::kMaxWavetableFrames);
                const int binCount = clampi(int(header.binCount), 1, synth::kMaxWavetableHarmonics);
                if(!isBank)
                    pushMetaUndoSnapshot();
                auto &metaSlot = track->metaOsc;
                auto &bank = track->partialBank;
                if(isBank)
                {
                    bank.frameCount = frameCount;
                    bank.morph = 0.0f;
                    bank.partialCount = synth::kMaxWavetablePartials;
                    for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
                        bank.partials[(size_t)i].enabled = true;
                }
                else
                {
                    metaSlot.frameCount = frameCount;
                }
                auto &frames = isBank ? bank.frames.ensure() : metaSlot.frames.ensure();
                const int limit = isBank ? synth::kMaxWavetablePartials : synth::kMaxWavetableHarmonics;
                for(int f = 0; f < frameCount; ++f)
                {
                    if(!frames[(size_t)f])
                        frames[(size_t)f] = std::make_shared<synth::WavetableFrame>();
                    auto &frame = *frames[(size_t)f];
                    frame.useImportedWaveform = false;
                    frame.waveform.reset();
                    frame.spectrum.reset();
                    frame.harmonics.fill(synth::WavetableHarmonic {});
                    for(int b = 0; b < binCount; ++b)
                    {
                        Kwt2PackedBin packed;
                        if(!bin.read(reinterpret_cast<char *>(&packed), sizeof(packed)))
                        {
                            loadStatus_ = "bad KWT2: " + path;
                            return false;
                        }
                        if(b < limit)
                            frame.harmonics[(size_t)b] = unpackKwtBin(packed, b);
                    }
                }
                selectedMetaFrame_ = 0;
                metaFrameSelected_.fill(false);
                metaFrameSelected_[0] = true;
                metaFrameRangeAnchor_ = 0;
                pushCurrentTrack();
                loadStatus_ = "loaded " + path;
                return true;
            }
        }

        std::ifstream in(path);
        if(!in)
        {
            loadStatus_ = "load failed: " + path;
            return false;
        }
        std::string tag;
        int version = 0;
        in >> tag >> version;
        if(tag != "KAPIBARA_WT")
        {
            loadStatus_ = "not a Kapibara wavetable: " + path;
            return false;
        }
        std::string key;
        int frameCount = 1;
        in >> key >> frameCount;
        frameCount = clampi(frameCount, 1, synth::kMaxWavetableFrames);

        if(!isBank)
            pushMetaUndoSnapshot();
        auto &metaSlot = track->metaOsc;
        auto &bank = track->partialBank;
        if(isBank)
        {
            bank.frameCount = frameCount;
            bank.morph = 0.0f;
            bank.partialCount = synth::kMaxWavetablePartials;
            for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
                bank.partials[(size_t)i].enabled = true;
        }
        else
        {
            metaSlot.frameCount = frameCount;
        }
        auto &frames = isBank ? bank.frames.ensure() : metaSlot.frames.ensure();
        for(int f = 0; f < frameCount; ++f)
        {
            std::string ftag;
            int idx = 0, used = 0;
            in >> ftag >> idx >> used;
            if(!frames[(size_t)f])
                frames[(size_t)f] = std::make_shared<synth::WavetableFrame>();
            auto &frame = *frames[(size_t)f];
            frame.useImportedWaveform = false;
            frame.waveform.reset();
            frame.spectrum.reset();
            frame.harmonics.fill(synth::WavetableHarmonic {});
            used = clampi(used, 0, synth::kMaxWavetableHarmonics);
            for(int h = 0; h < used; ++h)
            {
                float ratio = 1.0f, amp = 0.0f, phase = 0.0f;
                in >> ratio >> amp >> phase;
                const int dst = isBank ? clampi(int(std::round(ratio)) - 1, 0, synth::kMaxWavetablePartials)
                                       : h;
                if(dst >= (isBank ? synth::kMaxWavetablePartials : synth::kMaxWavetableHarmonics))
                    continue;
                frame.harmonics[(size_t)dst].ratio = isBank ? float(dst + 1) : ratio;
                frame.harmonics[(size_t)dst].amp = amp;
                frame.harmonics[(size_t)dst].phase = phase;
            }
            if(isBank)
                for(int h = 0; h < synth::kMaxWavetablePartials; ++h)
                    if(frame.harmonics[(size_t)h].ratio <= 0.0f)
                        frame.harmonics[(size_t)h].ratio = float(h + 1);
        }
        selectedMetaFrame_ = 0;
        metaFrameSelected_.fill(false);
        metaFrameSelected_[0] = true;
        metaFrameRangeAnchor_ = 0;
        pushCurrentTrack();
        loadStatus_ = "loaded " + path;
        return true;
    }

END_NAMESPACE_DISTRHO

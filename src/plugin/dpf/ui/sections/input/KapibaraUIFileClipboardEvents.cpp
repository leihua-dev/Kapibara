#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::uiFileBrowserSelected(const char *filename)
{
        if(filename == nullptr || filename[0] == '\0')
        {
            fileBrowserSaving_ = false;
            return;
        }
        if(fileBrowserSaving_)
        {
            fileBrowserSaving_ = false;
            saveWavetableToFile(filename);
            repaint();
            return;
        }
        if(samplerLoadPending_)
        {
            samplerLoadPending_ = false;
            if(auto *t = currentTrack();
               t != nullptr && t->type == synth::SourceTrackType::SampleNoise)
            {
                if(loadSampleIntoTrack(*t, filename))
                    pushCurrentTrack();
            }
            repaint();
            return;
        }
        loadPathBuffer_ = filename;
        commitWavetableLoad();
        repaint();
    }

uint32_t KapibaraUI::uiClipboardDataOffer()
{
        for(const auto &offer : getClipboardDataOfferTypes())
            if(offer.type != nullptr && std::strcmp(offer.type, "text/uri-list") == 0)
                return offer.id;
        return 0;
    }

void KapibaraUI::uiClipboardData(const char *mimeType, const void *data, size_t dataSize)
{
        if(mimeType == nullptr || std::strcmp(mimeType, "text/uri-list") != 0 || data == nullptr || dataSize == 0)
            return;
        const std::string uriList(static_cast<const char *>(data), dataSize);
        size_t offset = 0;
        while(offset < uriList.size())
        {
            const size_t end = uriList.find('\n', offset);
            const std::string line = uriList.substr(offset, end == std::string::npos ? std::string::npos : end - offset);
            offset = end == std::string::npos ? uriList.size() : end + 1;
            if(line.empty() || line[0] == '#')
                continue;
            const std::string path = localPathFromUri(line);
            if(!hasWavExtension(path))
                continue;
            auto *track = currentTrack();
            if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            {
                loadStatus_ = "select a Meta Oscillator before dropping WAV";
                metaEditorStatus_ = loadStatus_;
                repaint();
                return;
            }
            loadPathBuffer_ = path;
            droppedWavPending_ = true;
            wavetableImportMenuOpen_ = true;
            metaEditorStatus_ = "select WAV import mode";
            repaint();
            return;
        }
        loadStatus_ = "drop ignored: expected a .wav file";
        metaEditorStatus_ = loadStatus_;
        repaint();
    }

END_NAMESPACE_DISTRHO

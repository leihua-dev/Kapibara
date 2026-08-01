#include "../../../KapibaraUI.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

START_NAMESPACE_DISTRHO

// Text-prompt sample generation for the sampler.
//
// The plugin does not contain a model and does not want to: bundling an
// inference runtime would drag a large native dependency across four plugin
// formats and three platforms, and model weights carry their own licence which
// has no business inside a GPL tree. Instead this shells out to a generator the
// user configures, and then loads whatever WAV it produced through exactly the
// path a manual LOAD uses.
//
// The prompt is never interpolated into the command line. It is written to a
// file and only PATHS the plugin generated are substituted, so a prompt cannot
// turn into shell syntax no matter what is typed.

namespace
{
constexpr const char *kConfigPath = "presets/ai-generate.cmd";

const char *kDefaultConfig =
    "# Kapibara AI sample generation.\n"
    "# One shell command. These placeholders are substituted before running:\n"
    "#   {prompt_file}  a UTF-8 text file containing the prompt\n"
    "#   {out}          the .wav path the generator must write\n"
    "#   {seconds}      requested duration\n"
    "# The prompt is passed as a FILE, never inlined, so it cannot become shell\n"
    "# syntax. Edit the line below to point at your generator, e.g. a script\n"
    "# wrapping Stable Audio Open.\n"
    "kapibara-generate --prompt-file {prompt_file} --out {out} --seconds {seconds}\n";

std::string replaceAll(std::string s, const std::string &from, const std::string &to)
{
        if(from.empty())
            return s;
        for(size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size()))
            s.replace(at, from.size(), to);
        return s;
    }
} // namespace

std::string KapibaraUI::aiCommandTemplate()
{
        std::ifstream in(kConfigPath);
        if(in)
        {
            std::string line;
            while(std::getline(in, line))
            {
                // Skip comments and blanks; the first real line is the command.
                const size_t first = line.find_first_not_of(" \t\r\n");
                if(first == std::string::npos || line[first] == '#')
                    continue;
                return line.substr(first);
            }
        }
        // Write the template on first use so there is something to edit rather
        // than a feature that silently does nothing.
        std::error_code ec;
        std::filesystem::create_directories("presets", ec);
        std::ofstream out(kConfigPath, std::ios::trunc);
        if(out)
            out << kDefaultConfig;
        return std::string();
    }

void KapibaraUI::startAiSampleGeneration(synth::SourceTrackParams &track, const std::string &prompt)
{
        if(aiJob_ && aiJob_->state.load(std::memory_order_acquire) == 1)
        {
            metaEditorStatus_ = "AI: already generating";
            return;
        }
        if(prompt.empty())
        {
            metaEditorStatus_ = "AI: type a prompt first";
            return;
        }
        const std::string tmpl = aiCommandTemplate();
        if(tmpl.empty())
        {
            metaEditorStatus_ = std::string("AI: configure ") + kConfigPath;
            return;
        }

        std::error_code ec;
        const auto dir = std::filesystem::temp_directory_path(ec) / "kapibara-ai";
        std::filesystem::create_directories(dir, ec);
        const auto promptPath = dir / "prompt.txt";
        const auto outPath = dir / "generated.wav";
        {
            std::ofstream pf(promptPath, std::ios::trunc);
            if(!pf)
            {
                metaEditorStatus_ = "AI: cannot write prompt file";
                return;
            }
            pf << prompt;
        }
        std::filesystem::remove(outPath, ec);   // so a stale file can't look like success

        std::string cmd = tmpl;
        cmd = replaceAll(cmd, "{prompt_file}", promptPath.string());
        cmd = replaceAll(cmd, "{out}", outPath.string());
        cmd = replaceAll(cmd, "{seconds}", "8");

        auto job = std::make_shared<AiJob>();
        job->trackId = track.id;
        job->outPath = outPath.string();
        aiJob_ = job;
        metaEditorStatus_ = "AI: generating...";

        // Detached, and the job is captured by value: if this window closes
        // mid-generation the worker still has somewhere valid to write.
        std::thread([job, cmd]() {
            const int rc = std::system(cmd.c_str());
            std::error_code fec;
            const bool produced = std::filesystem::exists(job->outPath, fec)
                                  && std::filesystem::file_size(job->outPath, fec) > 0;
            if(rc == 0 && produced)
            {
                job->message = "AI: done";
                job->state.store(2, std::memory_order_release);
            }
            else
            {
                job->message = produced ? "AI: generator reported failure"
                                        : "AI: generator wrote no file";
                job->state.store(3, std::memory_order_release);
            }
        }).detach();
    }

void KapibaraUI::pollAiSampleGeneration()
{
        if(!aiJob_)
            return;
        const int st = aiJob_->state.load(std::memory_order_acquire);
        if(st == 1)
            return;
        auto job = aiJob_;
        aiJob_.reset();
        if(st != 2)
        {
            metaEditorStatus_ = job->message;
            repaint();
            return;
        }
        // Load on the UI thread, through the same path as a manual LOAD.
        for(auto &t : generator_.tracks)
            if(t.id == job->trackId)
            {
                if(loadSampleIntoTrack(t, job->outPath))
                {
                    t.sampler.sampleName = "AI: " + aiPromptBuffer_;
                    // The temp file is overwritten by the next generation, so a
                    // preset must not try to reload it later.
                    t.sampler.samplePath.clear();
                    pushTrackById(t.id);
                }
                break;
            }
        repaint();
    }

END_NAMESPACE_DISTRHO

#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

int KapibaraUI::envUseCount(int envIndex) const
{
        int count = 0;
        envIndex = clampi(envIndex, 0, synth::kMaxAmpEnvs - 1);
        for(const auto &track : generator_.tracks)
            if(clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1) == envIndex)
                ++count;
        return count;
    }

END_NAMESPACE_DISTRHO

#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

KapibaraUI::KapibaraUI()
    : KapibaraUIDrawing(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
{
#ifdef DGL_NO_SHARED_RESOURCES
        createFontFromFile("sans", "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf");
#else
        loadSharedResources();
#endif
        // Dual-font system (condensed labels + monospace numerals). Loaded from
        // common system paths; if missing, the helpers fall back to DejaVu sans.
        static const char *kCondPaths[] = {
            "/usr/share/fonts/TTF/DejaVuSansCondensed.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf",
        };
        static const char *kMonoPaths[] = {
            "/usr/share/fonts/TTF/JetBrainsMono-Regular.ttf",
            "/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        };
        for(const char *p : kCondPaths)
            if(!haveCondFont_ && createFontFromFile("cond", p) >= 0) haveCondFont_ = true;
        for(const char *p : kMonoPaths)
            if(!haveMonoFont_ && createFontFromFile("mono", p) >= 0) haveMonoFont_ = true;
        // Lock to a fixed 11:7 aspect ratio (matches the default 1320x840). The
        // min size must share that ratio or the window jumps ratio on resize.
        setGeometryConstraints(1100, 700, true);
        getWindow().setIgnoringKeyRepeat(true);
        computerKeys_.fill(false);
        pressedKeycodeNotes_.fill(-1);
        ampEnvRouteNodeSlots_.fill(255);
        pullFromPlugin();
        pushGroups();
    }

END_NAMESPACE_DISTRHO

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

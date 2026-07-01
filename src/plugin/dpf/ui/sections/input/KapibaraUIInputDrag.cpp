#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::applyDragValue(float x, float y)
{
    if(applySourceDragValue(x, y))
        return;
    if(applyOscillatorDragValue(x, y))
        return;
    if(applyModFxLayoutDragValue(x, y))
        return;
}


END_NAMESPACE_DISTRHO

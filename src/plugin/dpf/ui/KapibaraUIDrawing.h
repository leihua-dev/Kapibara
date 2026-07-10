#pragma once

class KapibaraUIDrawing : public UI
{
  public:
    KapibaraUIDrawing(uint width, uint height)
        : UI(width, height)
    {
    }

  protected:
    static constexpr float kCanvasW = 1320.0f;
    static constexpr float kCanvasH = 900.0f;
    static constexpr float kUiAspect = kCanvasW / kCanvasH;

    float uiW() const { return kCanvasW; }
    float uiH() const { return kCanvasH; }

    void updateLetterbox()
    {
        const float rw = realW_ > 1.0f ? realW_ : float(DISTRHO_UI_DEFAULT_WIDTH);
        const float rh = realH_ > 1.0f ? realH_ : float(DISTRHO_UI_DEFAULT_HEIGHT);
        uiRenderScale_ = std::min(rw / kCanvasW, rh / kCanvasH);
        lbW_ = kCanvasW * uiRenderScale_;
        lbH_ = kCanvasH * uiRenderScale_;
        lbX_ = (rw - lbW_) * 0.5f;
        lbY_ = (rh - lbH_) * 0.5f;
    }

    void useFallbackSans()
    {
#ifdef DGL_NO_SHARED_RESOURCES
        fontFace("sans");
#else
        fontFace(NANOVG_DEJAVU_SANS_TTF);
#endif
    }

    // Labels / captions: condensed sans, engineering-panel feel.
    void useUiFont()
    {
        if(haveCondFont_) fontFace("cond"); else useFallbackSans();
        textLetterSpacing(0.0f);
    }

    // Numeric readouts (values, frame/pitch coordinates): tabular monospace.
    void useMonoFont()
    {
        if(haveMonoFont_) fontFace("mono"); else useFallbackSans();
        textLetterSpacing(0.0f);
    }

    // Section headers: condensed, uppercase, widely tracked (~0.12em silk-screen).
    void useHeaderFont()
    {
        if(haveCondFont_) fontFace("cond"); else useFallbackSans();
        textLetterSpacing(1.3f);
    }

    void uiFontSize(float size)
    {
        fontSize(size * uiScale_);
    }

    void drawSectionTitle(float x, float y, const char *title)
    {
        beginPath();
        roundedRect(x, y + 2.0f, 2.0f, 14.0f, 1.0f);
        fillColor(DesignTokens::accentCyan());
        fill();
        useUiFont();
        uiFontSize(12.0f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(DesignTokens::textPrimary());
        text(x + 8.0f, y, title, nullptr);
    }

    // The machined knob graphic (cap, groove, ticks, value arc, pointer). `active`
    // brightens the value arc; the default state stays deliberately dim so the
    // numbers read louder than the controls.
    void drawKnobArc(float cx, float cy, float rad, float norm, bool active = false)
    {
        beginPath();
        circle(cx, cy, rad + 4.0f);
        fillPaint(linearGradient(cx, cy - rad - 4.0f, cx, cy + rad + 4.0f,
                                 shade(DesignTokens::controlBackground(), 0.08f),
                                 shade(DesignTokens::controlBackground(), -0.08f)));
        fill();
        strokeColor(DesignTokens::border());
        strokeWidth(DesignTokens::borderWidth);
        stroke();

        const float kStart = DesignTokens::knobStart;
        const float kEnd   = DesignTokens::knobStart + DesignTokens::knobSweep;
        const float kAngle = kStart + clampf(norm, 0.0f, 1.0f) * DesignTokens::knobSweep;

        lineCap(ROUND);
        beginPath();
        arc(cx, cy, rad, kStart, kEnd, CCW);
        strokeColor(DesignTokens::groove());
        strokeWidth(3.0f);
        stroke();

        for(int i = 0; i <= 4; ++i)
        {
            const float ta = kStart + (float(i) / 4.0f) * DesignTokens::knobSweep;
            const float tcos = std::cos(ta), tsin = std::sin(ta);
            strokeLine(cx + tcos * (rad + 2.0f), cy + tsin * (rad + 2.0f),
                       cx + tcos * (rad + 5.0f), cy + tsin * (rad + 5.0f),
                       DesignTokens::textSecondary().withAlpha(0.35f), 1.0f);
        }

        // Value arc — muted by default, bright cyan only when active/editing.
        if(norm > 0.001f)
        {
            beginPath();
            arc(cx, cy, rad, kStart, kAngle, CCW);
            strokeColor(active ? DesignTokens::accentCyan()
                               : DesignTokens::accentCyan().withAlpha(0.42f));
            strokeWidth(active ? 2.75f : 2.0f);
            stroke();
        }

        const float ax = std::cos(kAngle);
        const float ay = std::sin(kAngle);
        beginPath();
        moveTo(cx + ax * rad * 0.30f, cy + ay * rad * 0.30f);
        lineTo(cx + ax * rad * 0.68f, cy + ay * rad * 0.68f);
        strokeColor(DesignTokens::textPrimary().withAlpha(active ? 1.0f : 0.8f));
        strokeWidth(1.75f);
        stroke();
        lineCap(BUTT);
    }

    void drawKnob(const Rect &r, const char *label, float norm, float value)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.3g", value);
        drawKnobLabeled(r, label, norm, buf);
    }

    // Knob with a caller-formatted value string (fixed decimals, units, etc.).
    void drawKnobLabeled(const Rect &r, const char *label, float norm, const char *buf)
    {
        const bool tall = r.h >= r.w * 0.65f;
        const float sz   = tall ? std::min(r.w, r.h) : r.h;
        const float rad  = std::max(6.0f, sz * 0.5f - 8.0f);
        const float cx   = tall ? r.x + r.w * 0.5f : r.x + sz * 0.5f;
        const float cy   = tall ? r.y + sz * 0.5f + 2.0f : r.y + r.h * 0.5f;

        drawKnobArc(cx, cy, rad, norm);

        if(tall)
        {
            useUiFont();
            uiFontSize(10.5f);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            fillColor(DesignTokens::textSecondary());
            text(cx, r.y + sz + 2.0f, label, nullptr);
            useMonoFont();
            uiFontSize(11.5f);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            fillColor(DesignTokens::textPrimary());
            text(cx, r.y + sz + 14.0f, buf, nullptr);
        }
        else
        {
            const float tx = r.x + sz + 5.0f;
            useUiFont();
            uiFontSize(10.5f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary());
            text(tx, cy - 5.5f, label, nullptr);
            useMonoFont();
            uiFontSize(12.0f);
            fillColor(DesignTokens::textPrimary());
            text(tx, cy + 6.0f, buf, nullptr);
        }
    }

    // Instrument-panel parameter row: knob at the left, condensed label, and a
    // right-aligned monospace value on one baseline — a strict aligned column.
    void drawParamKnobRow(const Rect &r, const char *label, float norm, const char *valueText,
                          bool active = false)
    {
        const float ksz = std::min(r.h, 30.0f);
        const float rad = std::max(6.0f, ksz * 0.5f - 5.0f);
        const float cx  = r.x + ksz * 0.5f;
        const float cy  = r.y + r.h * 0.5f;
        drawKnobArc(cx, cy, rad, norm, active);

        useUiFont();
        uiFontSize(11.5f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(DesignTokens::textSecondary());
        text(r.x + ksz + 10.0f, cy, label, nullptr);

        useMonoFont();
        uiFontSize(13.0f);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        fillColor(DesignTokens::textPrimary());
        text(r.x + r.w, cy, valueText, nullptr);
    }

    // Small uppercase section label (PITCH / WAVETABLE / UNISON) — an instrument
    // panel silk-screen caption, drawn above a group rather than boxing it.
    void drawGroupLabel(float x, float y, const char *label)
    {
        useHeaderFont();
        uiFontSize(10.0f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        // Dimmer than parameter labels so headers recede, not dominate.
        fillColor(DesignTokens::textSecondary().withAlpha(0.55f));
        text(x, y, label, nullptr);
    }

    // Compact integer stepper: [-] value [+]. The two button rects are returned so
    // the caller can hit-test them.
    void drawStepper(const Rect &r, const char *label, int value, Rect &downRect, Rect &upRect)
    {
        useUiFont();
        uiFontSize(9.0f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(DesignTokens::textSecondary());
        text(r.x, r.y, label, nullptr);

        const float by = r.y + 13.0f;
        const float bh = std::max(16.0f, r.h - 15.0f);
        const float bw = std::min(22.0f, r.w * 0.28f);
        downRect = { r.x, by, bw, bh };
        upRect   = { r.x + r.w - bw, by, bw, bh };
        drawButton(downRect, "-", false);
        drawButton(upRect, "+", false);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", value);
        useMonoFont();
        uiFontSize(14.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(DesignTokens::textPrimary());
        text(r.x + r.w * 0.5f, by + bh * 0.5f, buf, nullptr);
    }

    void drawSlider(const Rect &r, const char *label, float norm, float value)
    {
        if(r.w < 80.0f || r.h > r.w * 0.55f)
        {
            drawKnob(r, label, norm, value);
            return;
        }

        const float y = r.y + r.h * 0.5f;
        beginPath();
        roundedRect(r.x, y - 1.0f, r.w, 2.0f, 1.0f);
        fillColor(DesignTokens::divider());
        fill();
        beginPath();
        roundedRect(r.x, y - 1.25f, clampf(norm, 0.0f, 1.0f) * r.w, 2.5f, 1.25f);
        fillColor(DesignTokens::accentCyan());
        fill();
        const float hx = r.x + clampf(norm, 0.0f, 1.0f) * r.w;
        beginPath();
        circle(hx, y, 4.0f);
        fillColor(DesignTokens::accentCyan());
        fill();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.3g", value);
        useUiFont();
        uiFontSize(10.5f);
        textAlign(ALIGN_LEFT | ALIGN_BOTTOM);
        fillColor(DesignTokens::textSecondary());
        text(r.x, r.y - 2.0f, label, nullptr);
        useMonoFont();
        textAlign(ALIGN_RIGHT | ALIGN_BOTTOM);
        fillColor(DesignTokens::textPrimary());
        text(r.x + r.w, r.y - 2.0f, buf, nullptr);
    }

    void drawLabelBox(const Rect &r, const char *textValue)
    {
        drawPanel(r, DesignTokens::controlBackground(), DesignTokens::border());
        useUiFont();
        uiFontSize(13.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(DesignTokens::textPrimary());
        text(r.x + 10.0f, r.y + r.h * 0.5f, textValue, nullptr);
    }

    void drawButton(const Rect &r, const char *label, bool active)
    {
        const float radius = std::min(DesignTokens::controlRadius, std::min(r.w, r.h) * 0.45f);
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, radius);
        fillColor(active ? DesignTokens::panelRaised() : DesignTokens::controlBackground());
        fill();
        beginPath();
        roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f, radius);
        strokeColor(active ? DesignTokens::accentCyan() : DesignTokens::border());
        strokeWidth(DesignTokens::borderWidth);
        stroke();
        useUiFont();
        uiFontSize(12.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(active ? DesignTokens::textPrimary() : DesignTokens::textSecondary());
        text(r.x + r.w * 0.5f, r.y + r.h * 0.5f, label, nullptr);
    }

    void drawDropdown(const Rect &r, const char *label, bool open)
    {
        const float radius = std::min(DesignTokens::controlRadius, std::min(r.w, r.h) * 0.45f);
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, radius);
        fillColor(open ? DesignTokens::panelRaised() : DesignTokens::controlBackground());
        fill();
        beginPath();
        roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f, radius);
        strokeColor(open ? DesignTokens::accentCyan() : DesignTokens::border());
        strokeWidth(DesignTokens::borderWidth);
        stroke();
        useUiFont();
        uiFontSize(12.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(open ? DesignTokens::textPrimary() : DesignTokens::textSecondary());
        scissor(r.x + 10.0f, r.y, std::max(10.0f, r.w - 32.0f), r.h);
        text(r.x + 10.0f, r.y + r.h * 0.5f + 0.5f, label, nullptr);
        resetScissor();
        const float chx = r.x + r.w - 15.0f;
        const float chy = r.y + r.h * 0.5f;
        beginPath();
        moveTo(chx - 4.0f, chy - 2.0f);
        lineTo(chx, chy + 3.0f);
        lineTo(chx + 4.0f, chy - 2.0f);
        strokeColor(open ? DesignTokens::accentCyan() : DesignTokens::textSecondary());
        strokeWidth(1.5f);
        lineCap(ROUND);
        stroke();
        lineCap(BUTT);
    }

    void drawPanel(const Rect &r, Color fillValue, Color strokeValue)
    {
        // Flat machined-metal panel: fill + one crisp border, no glossy top
        // highlight — big blocks read via their border/divider, not a layered
        // rounded-card look.
        const float radius = std::min(DesignTokens::panelRadius, std::min(r.w, r.h) * 0.45f);
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, radius);
        fillColor(fillValue);
        fill();
        beginPath();
        roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f, radius);
        strokeColor(strokeValue);
        strokeWidth(DesignTokens::borderWidth);
        stroke();
    }

    void drawTabBar(const Rect &r, const char *const *labels, int count, int selected, Rect *rects)
    {
        if(count <= 0)
            return;
        const float tabW = r.w / float(count);
        useUiFont();
        uiFontSize(11.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        for(int i = 0; i < count; ++i)
        {
            const Rect tab { r.x + tabW * float(i), r.y, tabW, r.h };
            if(rects != nullptr)
                rects[i] = tab;
            fillColor(i == selected ? DesignTokens::textPrimary() : DesignTokens::textSecondary());
            text(tab.x + tab.w * 0.5f, tab.y + tab.h * 0.45f, labels[i], nullptr);
            if(i == selected)
            {
                beginPath();
                roundedRect(tab.x + 8.0f, tab.y + tab.h - 2.0f, std::max(4.0f, tab.w - 16.0f), 2.0f, 1.0f);
                fillColor(DesignTokens::accentCyan());
                fill();
            }
        }
    }

    void strokeLine(float x1, float y1, float x2, float y2, Color color, float width)
    {
        beginPath();
        moveTo(x1, y1);
        lineTo(x2, y2);
        strokeColor(color);
        strokeWidth(width);
        stroke();
    }

    const char *buttonText(const char *fmt, int value)
    {
        std::snprintf(scratch_, sizeof(scratch_), fmt, value);
        return scratch_;
    }

    const char *buttonText(const char *fmt, int a, int b)
    {
        std::snprintf(scratch_, sizeof(scratch_), fmt, a, b);
        return scratch_;
    }

    // Dual-font system: condensed sans for labels/headers, monospace for numbers.
    // Loaded from system TTFs at startup; both fall back to the DejaVu sans face if
    // unavailable so text never disappears.
    bool haveCondFont_ = false;
    bool haveMonoFont_ = false;

    float uiScale_ = 1.0f;
    char scratch_[64] {};
    float realW_ = float(DISTRHO_UI_DEFAULT_WIDTH);
    float realH_ = float(DISTRHO_UI_DEFAULT_HEIGHT);
    float lbX_ = 0.0f;
    float lbY_ = 0.0f;
    float lbW_ = float(DISTRHO_UI_DEFAULT_WIDTH);
    float lbH_ = float(DISTRHO_UI_DEFAULT_HEIGHT);
    float uiRenderScale_ = 1.0f;
};

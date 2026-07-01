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

    void useUiFont()
    {
#ifdef DGL_NO_SHARED_RESOURCES
        fontFace("sans");
#else
        fontFace(NANOVG_DEJAVU_SANS_TTF);
#endif
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

    void drawKnob(const Rect &r, const char *label, float norm, float value)
    {
        const bool tall = r.h >= r.w * 0.65f;
        const float sz   = tall ? std::min(r.w, r.h) : r.h;
        const float rad  = std::max(6.0f, sz * 0.5f - 8.0f);
        const float cx   = tall ? r.x + r.w * 0.5f : r.x + sz * 0.5f;
        const float cy   = tall ? r.y + sz * 0.5f + 2.0f : r.y + r.h * 0.5f;

        beginPath();
        circle(cx, cy, rad + 4.0f);
        fillPaint(linearGradient(cx, cy - rad - 4.0f, cx, cy + rad + 4.0f,
                                 shade(DesignTokens::controlBackground(), 0.20f),
                                 shade(DesignTokens::controlBackground(), -0.16f)));
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
        strokeColor(DesignTokens::divider());
        strokeWidth(2.5f);
        stroke();
        if(norm > 0.001f)
        {
            beginPath();
            arc(cx, cy, rad, kStart, kAngle, CCW);
            strokeColor(DesignTokens::accentCyan().withAlpha(0.22f));
            strokeWidth(6.0f);
            stroke();
            beginPath();
            arc(cx, cy, rad, kStart, kAngle, CCW);
            strokeColor(DesignTokens::accentCyan());
            strokeWidth(2.75f);
            stroke();
        }

        const float ax = std::cos(kAngle);
        const float ay = std::sin(kAngle);
        const float p0 = rad * 0.30f;
        const float p1 = rad * 0.68f;
        beginPath();
        moveTo(cx + ax * p0, cy + ay * p0);
        lineTo(cx + ax * p1, cy + ay * p1);
        strokeColor(DesignTokens::textPrimary());
        strokeWidth(1.75f);
        stroke();
        lineCap(BUTT);
        beginPath();
        circle(cx + ax * p1, cy + ay * p1, 1.7f);
        fillColor(DesignTokens::accentCyan());
        fill();

        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.3g", value);
        useUiFont();
        if(tall)
        {
            uiFontSize(10.5f);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            fillColor(DesignTokens::textSecondary());
            text(cx, r.y + sz + 2.0f, label, nullptr);
            uiFontSize(11.5f);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            fillColor(DesignTokens::textPrimary());
            text(cx, r.y + sz + 14.0f, buf, nullptr);
        }
        else
        {
            const float tx = r.x + sz + 5.0f;
            uiFontSize(10.5f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary());
            text(tx, cy - 5.5f, label, nullptr);
            uiFontSize(12.0f);
            fillColor(DesignTokens::textPrimary());
            text(tx, cy + 6.0f, buf, nullptr);
        }
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
        const float radius = std::min(DesignTokens::panelRadius, std::min(r.w, r.h) * 0.45f);
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, radius);
        fillColor(fillValue);
        fill();
        if(r.h > 8.0f && r.w > radius * 2.0f + 4.0f)
        {
            beginPath();
            moveTo(r.x + radius, r.y + 1.0f);
            lineTo(r.x + r.w - radius, r.y + 1.0f);
            strokeColor(shade(fillValue, 0.16f));
            strokeWidth(1.0f);
            stroke();
        }
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

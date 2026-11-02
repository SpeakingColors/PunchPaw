#pragma once
#include "Widgets.h"
#include "../core/PunchPaw.h"

namespace ui
{
// Everything the views draw, refreshed by the editor timer (message thread only).
struct Live
{
    double fs = 44100.0;
    float inPk = 0, outPk = 0;       // linear, decaying peak hold
    float grNow = 0;                 // dB, positive
    float vuI = 0, vuO = 0;          // mean square (65 ms), for the dial
    float over = -99.0f;             // detector minus threshold, dB
    float atkNow = 15.0f;            // auto attack right now, ms
    // parameters in real units
    pp::Params p;
    bool bypass = false;
    int view = 0;                    // dial shows 0 in, 1 out, 2 gain reduction
    float hist[360] {}; int hi = 0;
    float scIn[pp::kScopeN] {}, scOut[pp::kScopeN] {}, scGr[pp::kScopeN] {}; int scI = 0;
};

inline float l2d (float x) { return 20.0f * std::log10 (std::max (x, 1.0e-9f)); }
inline float msDb (float ms) { return 10.0f * std::log10 (ms + 1.0e-12f); }

// Base of the display canvases: drawn in the mockup's canvas units, scaled into a bordered box.
class CanvasView : public Component, public TipClient
{
public:
    CanvasView (const Ctx& c, const Live& l, float cw, float ch) : ctx (c), live (l), cvW (cw), cvH (ch) {}
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto r = getLocalBounds().toFloat();
        g.setColour (t.disp);
        g.fillRoundedRectangle (r, 3.0f);
        auto inner = r.reduced (1.0f);
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (inner.toNearestInt());
            g.addTransform (AffineTransform::scale (inner.getWidth() / cvW, inner.getHeight() / cvH).translated (inner.getX(), inner.getY()));
            draw (g, t, cvW, cvH);
        }
        g.setColour (t.line);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    }
protected:
    virtual void draw (Graphics&, const theme::Look&, float w, float h) = 0;
    static void label (Graphics& g, const String& s, float x, float y, Justification j, Colour c, float px = 9.0f)
    {
        g.setFont (theme::font (theme::FontKind::mono, px));
        g.setColour (c);
        g.drawSingleLineText (s, roundToInt (x), roundToInt (y), j);
    }
    static void dashedLine (Graphics& g, Line<float> ln, float a, float b, float w)
    {
        const float d[2] = { a, b };
        g.drawDashedLine (ln, d, 2, w);
    }
    const Ctx& ctx;
    const Live& live;
    float cvW, cvH;
};

// Analogue style level meter: input, output or gain reduction (320 x 124).
class DialView : public CanvasView
{
public:
    DialView (const Ctx& c, const Live& l) : CanvasView (c, l, 320, 124) {}
protected:
    static void paw (Graphics& g, float x, float y, float s, Colour col)
    {
        g.setColour (col);
        g.fillEllipse (x - s * 0.55f, y + s * 0.3f - s * 0.45f, s * 1.1f, s * 0.9f);
        const float d[4][2] = { { -0.85f, -0.3f }, { -0.3f, -0.85f }, { 0.3f, -0.85f }, { 0.85f, -0.3f } };
        for (auto& p : d)
        {
            Path e; e.addEllipse (-s * 0.2f, -s * 0.3f, s * 0.4f, s * 0.6f);
            g.fillPath (e, AffineTransform::rotation (p[0] * 0.6f).translated (x + p[0] * s, y + p[1] * s));
        }
    }
    void draw (Graphics& g, const theme::Look& t, float, float) override
    {
        const bool gr = live.view == 2;
        const float mn = gr ? 0.0f : -40.0f, mx = gr ? 30.0f : 3.0f, cx = 160, cy = 170, R = 134, a0 = -50, a1 = 50;
        auto ang = [=] (float v) { return degreesToRadians (a0 + (jlimit (mn, mx, v) - mn) / (mx - mn) * (a1 - a0)); };
        paw (g, 160, 84, 22, Colour::fromFloatRGBA (1.0f, 123 / 255.0f, 177 / 255.0f, 0.07f));
        struct Band { float v0, v1; Colour c; };
        const Band bands[3] = { { gr ? 0.0f : -40.0f, gr ? 10.0f : -12.0f, theme::blue }, { gr ? 10.0f : -12.0f, gr ? 20.0f : 0.0f, theme::purple }, { gr ? 20.0f : 0.0f, gr ? 30.0f : 3.0f, theme::red } };
        for (auto& b : bands)
        {
            Path arc; arc.addCentredArc (cx, cy, R - 3, R - 3, 0.0f, ang (b.v0), ang (b.v1), true);
            g.setColour (b.c);
            g.strokePath (arc, PathStrokeType (5.0f, PathStrokeType::mitered, PathStrokeType::butt));
        }
        const std::vector<float> maj = gr ? std::vector<float> { 0, 3, 6, 10, 15, 20, 30 } : std::vector<float> { -40, -30, -20, -12, -6, 0, 3 };
        const float step = gr ? 1.0f : 2.0f;
        for (float v = mn; v <= mx + 0.01f; v += step)
        {
            const float a = ang (v);
            const bool isM = std::find (maj.begin(), maj.end(), v) != maj.end();
            g.setColour (isM ? t.ink : t.dim);
            g.drawLine (cx + std::sin (a) * (R - (isM ? 16.0f : 12.0f)), cy - std::cos (a) * (R - (isM ? 16.0f : 12.0f)), cx + std::sin (a) * (R - 7), cy - std::cos (a) * (R - 7), 1.0f);
        }
        g.setFont (theme::font (theme::FontKind::mono, 10.0f));
        g.setColour (t.ink);
        for (float v : maj)
        {
            const float a = ang (v);
            g.drawSingleLineText (String ((gr || v <= 0) ? "" : "+") + String ((int) v), roundToInt (cx + std::sin (a) * (R - 27)), roundToInt (cy - std::cos (a) * (R - 27) + 3), Justification::horizontallyCentred);
        }
        const float val = gr ? live.grNow : msDb (live.view == 0 ? live.vuI : live.vuO);
        const float a = ang (val);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawLine (cx + 2, cy + 2, cx + 2 + std::sin (a) * (R - 12), cy + 2 - std::cos (a) * (R - 12), 3.0f);
        g.setColour (t.ink);
        g.drawLine (cx, cy, cx + std::sin (a) * (R - 12), cy - std::cos (a) * (R - 12), 2.0f);
        g.setFont (theme::font (theme::FontKind::bold, 11.0f));
        g.setColour (t.title);
        g.drawSingleLineText (gr ? "GAIN REDUCTION" : live.view == 0 ? "INPUT" : "OUTPUT", 8, 14);
        g.setFont (theme::font (theme::FontKind::mono, 12.0f));
        g.setColour (t.ink);
        g.drawSingleLineText ((val <= -60.0f ? String ("-inf") : String (val, 1)) + " dB", 8, 28);
        g.setFont (theme::font (theme::FontKind::brand, 12.0f));
        g.setColour (t.dim);
        g.drawSingleLineText ("PUNCHPAW", 312, 116, Justification::right);
    }
};

// Transfer curve: level in against level out (270 x 100).
class CurveView : public CanvasView
{
public:
    CurveView (const Ctx& c, const Live& l) : CanvasView (c, l, 270, 100) {}
protected:
    static float outOf (float v, const pp::Params& p)
    {
        const float gr = pp::grOf (v - p.thrDb, p.slope, p.soft);
        return v - std::min (60.0f, gr) + p.outDb;
    }
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        const float x0 = -60, x1 = 6, y0 = -60, y1 = 12;
        auto X = [=] (float v) { return 26.0f + (v - x0) / (x1 - x0) * (w - 32.0f); };
        auto Y = [=] (float v) { return h - 14.0f - (v - y0) / (y1 - y0) * (h - 22.0f); };
        for (float v : { -60.0f, -48.0f, -36.0f, -24.0f, -12.0f, 0.0f })
        {
            g.setColour (t.line); g.drawLine (X (v), 4, X (v), h - 14, 1.0f);
            label (g, String ((int) v), X (v), h - 4, Justification::horizontallyCentred, t.dim);
        }
        for (float v : { -48.0f, -24.0f, 0.0f, 12.0f })
        {
            g.setColour (t.line); g.drawLine (26, Y (v), w - 6, Y (v), 1.0f);
            label (g, String ((int) v), 23, Y (v) + 3, Justification::right, t.dim);
        }
        g.setColour (Colour (0xff4a4d55));
        dashedLine (g, { X (-60), Y (-60), X (6), Y (6) }, 3, 3, 1.0f);
        const auto& p = live.p;
        const bool ceilOn = p.ceilDb < 50.0f;
        if (ceilOn)
        {
            g.setColour (theme::pink);
            dashedLine (g, { 26, Y (p.ceilDb), w - 6, Y (p.ceilDb) }, 5, 3, 1.2f);
        }
        Path curve;
        bool first = true;
        for (float v = x0; v <= x1 + 0.01f; v += 0.5f)
        {
            float o = live.bypass ? v : outOf (v, p);
            if (! live.bypass && p.mix < 1.0f)
                o = 10.0f * std::log10 (std::pow (10.0f, v / 10.0f) * (1.0f - p.mix) + std::pow (10.0f, o / 10.0f) * p.mix);
            if (! live.bypass && ceilOn)
            {
                const float lim = p.ceilDb;
                if (o > lim - 4.0f) o = l2d (pp::softClip (pp::dbToLin (o), pp::dbToLin (p.ceilDb)));
            }
            const Point<float> pt (X (v), Y (jlimit (y0, y1, o)));
            if (first) curve.startNewSubPath (pt); else curve.lineTo (pt);
            first = false;
        }
        g.setColour (t.accent);
        g.strokePath (curve, PathStrokeType (2.0f));
        const float li = msDb (live.vuI), lo = msDb (live.vuO);
        if (li > -60.0f)
        {
            g.setColour (Colours::white);
            g.fillEllipse (X (jmin (x1, li)) - 4, Y (jlimit (y0, y1, lo)) - 4, 8, 8);
        }
    }
};

// Punch shape: input and output loudness over the last 1.2 s, with the gain reduction bars (430 x 100).
class ShapeView : public CanvasView
{
public:
    ShapeView (const Ctx& c, const Live& l) : CanvasView (c, l, 430, 100) {}
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        auto Y = [=] (float db) { return h - 14.0f - (jlimit (-48.0f, 0.0f, db) + 48.0f) / 48.0f * (h - 22.0f); };
        const int N = pp::kScopeN;
        auto X = [=] (int i) { return 28.0f + (float) i / (float) (N - 1) * (w - 34.0f); };
        for (float d : { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f })
        {
            g.setColour (t.line); g.drawLine (28, Y (d), w - 4, Y (d), 1.0f);
            label (g, String ((int) d), 25, Y (d) + 3, Justification::right, t.dim);
        }
        for (float tt : { 0.4f, 0.8f, 1.2f })
        {
            const float x = 28.0f + (tt / 1.2f) * (w - 34.0f);
            label (g, "+" + String (tt, 1) + " s", jmin (x, w - 14.0f), h - 3, Justification::horizontallyCentred, t.dim);
        }
        auto idx = [&] (int i) { return (live.scI + i) % N; };
        auto path = [&] (const float* arr, bool closed)
        {
            Path p;
            p.startNewSubPath (X (0), Y (-48));
            for (int i = 0; i < N; ++i) p.lineTo (X (i), Y (l2d (arr[idx (i)])));
            p.lineTo (X (N - 1), Y (-48));
            if (closed) p.closeSubPath();
            return p;
        };
        g.setColour (Colour::fromFloatRGBA (169 / 255.0f, 173 / 255.0f, 184 / 255.0f, 0.30f));
        g.fillPath (path (live.scIn, true));
        g.setColour (t.accent.withAlpha (0.45f));
        g.fillPath (path (live.scOut, true));
        Path line;
        for (int i = 0; i < N; ++i) { const Point<float> pt (X (i), Y (l2d (live.scOut[idx (i)]))); if (i == 0) line.startNewSubPath (pt); else line.lineTo (pt); }
        g.setColour (t.accent);
        g.strokePath (line, PathStrokeType (1.3f));
        g.setColour (Colour::fromFloatRGBA (1.0f, 123 / 255.0f, 177 / 255.0f, 0.65f));
        for (int i = 0; i < N; ++i)
        {
            const float gr = live.scGr[idx (i)];
            if (gr > 0.1f) g.fillRect (X (i) - 0.6f, 4.0f, 1.4f, jmin (1.0f, gr / 24.0f) * (h - 30.0f));
        }
    }
};

// Gain reduction over the last 6 s (268 x 100).
class HistView : public CanvasView
{
public:
    HistView (const Ctx& c, const Live& l) : CanvasView (c, l, 268, 100) {}
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        auto Y = [=] (float d) { return 8.0f + d / 30.0f * (h - 22.0f); };
        for (int d = 0; d <= 30; d += 6)
        {
            g.setColour (t.line); g.drawLine (26, Y ((float) d), w - 4, Y ((float) d), 1.0f);
            label (g, "-" + String (d), 23, Y ((float) d) + 3, Justification::right, t.dim);
        }
        Path p;
        p.startNewSubPath (28, Y (0));
        for (int i = 0; i < 360; ++i)
            p.lineTo (28.0f + (float) i / 359.0f * (w - 32.0f), Y (jmin (30.0f, live.hist[(live.hi + i) % 360])));
        p.lineTo (w - 4, Y (0));
        p.closeSubPath();
        g.setColour (Colour::fromFloatRGBA (164 / 255.0f, 61 / 255.0f, 1.0f, 0.35f));
        g.fillPath (p);
        g.setColour (theme::selb);
        g.strokePath (p, PathStrokeType (1.6f));
        label (g, "GR " + String (live.grNow, 1) + " dB", 34, h - 6, Justification::left, t.ink, 11.0f);
        label (g, live.p.manual ? "Atk " + fmt (live.p.atkMs) + "  Rel " + fmt (live.p.relMs) : String ("Auto timing"), w - 6, h - 6, Justification::right, t.dim, 11.0f);
    }
    static String fmt (float ms)
    {
        if (ms >= 1000.0f) return String (ms / 1000.0f, 2) + " s";
        if (ms >= 100.0f) return String (roundToInt (ms)) + " ms";
        return String (ms, ms >= 10.0f ? 1 : 2) + " ms";
    }
};

// Input or output level strip, 38 x 300. Click to clear the clip marker.
class MeterView : public Component, public TipClient
{
public:
    MeterView (const Ctx& c) : ctx (c) {}
    float lin = 0.0f;
    bool latch = false;
    void update (float l)
    {
        lin = l;
        if (l2d (l) >= 0.0f) latch = true;
        repaint();
    }
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        const float w = (float) getWidth(), h = (float) getHeight(), mn = -60, mx = 6;
        auto y = [=] (float db) { return h - 6.0f - (jlimit (mn, mx, db) - mn) / (mx - mn) * (h - 12.0f); };
        g.fillAll (t.disp);
        const float db = l2d (lin), top = y (db);
        auto f = [=] (float d) { return (d - mn) / (mx - mn); };
        ColourGradient grad (theme::blue, 0.0f, y (mn), theme::red, 0.0f, y (mx), false);
        grad.addColour (f (-12.0f), theme::blue);
        grad.addColour (f (-12.0f) + 0.001, theme::purple);
        grad.addColour (f (0.0f), theme::purple);
        grad.addColour (f (0.0f) + 0.001, theme::red);
        g.setGradientFill (grad);
        g.fillRect (2.0f, top, 12.0f, y (mn) - top);
        g.setFont (theme::font (theme::FontKind::mono, 9.0f));
        for (float tk : { 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f })
        {
            const float yy = y (tk);
            g.setColour (t.dim);
            g.fillRect (14.0f, yy, 3.0f, 1.0f);
            g.drawSingleLineText (String ((int) tk), roundToInt (w - 1.0f), roundToInt (yy + 3.0f), Justification::right);
        }
        g.setColour (latch ? theme::red : Colour (0xff2a2c31));
        g.fillRect (2.0f, 0.0f, 12.0f, 4.0f);
    }
    void mouseDown (const MouseEvent&) override { latch = false; repaint(); }
private:
    const Ctx& ctx;
};

// The top plate: labels, LEDs and read-outs. Click areas are child components.
class PlateView : public Component
{
public:
    PlateView (const Ctx& c, const Live& l) : ctx (c), live (l) {}
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        g.addTransform (AffineTransform::translation (-layout::top.x, -layout::top.y));
        for (int i = 0; i < layout::numPlateTexts; ++i)
        {
            if (live.p.manual && i >= layout::autoNoteFirst && i < layout::autoNoteFirst + 2) continue;
            auto& x = layout::plateTexts[i];
            String s (x.s);
            if (x.upper) s = s.toUpperCase();
            const auto kind = x.font == 2 ? theme::FontKind::mono : x.font == 1 ? theme::FontKind::bold : theme::FontKind::regular;
            const Colour col = x.role == 0 ? t.title : x.role == 1 ? t.ink : t.dim;
            drawText (g, s, rc (x.r), theme::font (kind, x.size), col, Justification::centred, x.spacing);
        }
        if (! live.p.manual)
        {
            const auto f = theme::font (theme::FontKind::mono, 12.0f);
            auto row = [&] (const layout::Txt& x, const String& name, const String& mid, const String& val)
            {
                float px = x.r.x;
                const float cy = x.r.y;
                auto put = [&] (const String& s, Colour c) { const float w = textWidth (f, s); drawText (g, s, { px, cy, w, x.r.h }, f, c, Justification::centredLeft); px += w; };
                put (name, t.title); put (mid, t.dim); put (val, t.ink);
            };
            row (layout::autoRead[0], "Attack", " now ", String (live.atkNow, 1) + " ms");
            row (layout::autoRead[1], "Release", " ", String (roundToInt (pp::kAutoReleaseDbPerSec)) + " dB/s");
        }
        auto led = [&] (const layout::Led& l, bool on) { drawLed (g, l, on, t); };
        for (int i = 0; i < 3; ++i) led (layout::viewLeds[i], live.view == i);
        static const float lad[12] = { 1, 2, 3, 4, 6, 8, 10, 15, 20, 30, 40, 60 };
        for (int i = 0; i < 12; ++i) led (layout::ladder[i], live.grNow >= lad[i] - 0.25f);
        for (int i = 0; i < 2; ++i) led (layout::timingLeds[i], (live.p.manual ? 1 : 0) == i);
        for (int i = 0; i < 2; ++i) led (layout::kneeLeds[i], (live.p.soft ? 1 : 0) == i);
        const int mode = live.grNow > 0.3f ? 2 : (live.over > -6.0f ? 1 : 0);
        for (int i = 0; i < 3; ++i) led (layout::thrLeds[i], mode == i);
    }
private:
    static void drawLed (Graphics& g, const layout::Led& l, bool on, const theme::Look& t)
    {
        const Colour c = theme::ledColour (l.color, t);
        if (on)
        {
            ColourGradient glow (c.withAlpha (0.6f), l.cx, l.cy, c.withAlpha (0.0f), l.cx + 12.5f, l.cy, true);
            glow.addColour (6.0 / 12.5, c.withAlpha (0.55f));
            g.setGradientFill (glow);
            g.fillEllipse (l.cx - 12.5f, l.cy - 12.5f, 25.0f, 25.0f);
            g.setColour (c);
            g.fillEllipse (l.cx - 6.0f, l.cy - 6.0f, 12.0f, 12.0f);
            g.setColour (Colours::white.withAlpha (0.35f));
            g.drawEllipse (l.cx - 5.0f, l.cy - 5.0f, 10.0f, 10.0f, 1.0f);
        }
        else
        {
            g.setColour (Colour (0xff26282c));
            g.fillEllipse (l.cx - 6.0f, l.cy - 6.0f, 12.0f, 12.0f);
            g.setColour (Colours::black.withAlpha (0.55f));
            g.drawEllipse (l.cx - 5.5f, l.cy - 5.5f, 11.0f, 11.0f, 1.0f);
        }
    }
    const Ctx& ctx;
    const Live& live;
};
} // namespace ui

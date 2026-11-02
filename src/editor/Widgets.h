#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "Theme.h"
#include "Layout.h"

namespace ui
{
using namespace juce;

// Settings every widget needs to paint (look and info bubbles).
struct Ctx
{
    char look = 'C';
    bool tips = true;
    bool power = false;
    const theme::Look& t() const { return theme::look (look); }
};

// Anything that can show an info bubble.
struct TipClient
{
    String tip;
    virtual ~TipClient() = default;
};

inline Rectangle<float> rc (const layout::Rc& r) { return { r.x, r.y, r.w, r.h }; }

inline float textWidth (const Font& f, const String& s)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// Text centred in a box, never shortened with dots. Letter spacing is in px.
inline void drawText (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c,
                      Justification j = Justification::centred, float spacingPx = 0.0f)
{
    if (std::abs (spacingPx) > 1.0e-6f) f = f.withExtraKerningFactor (spacingPx / jmax (1.0f, f.getHeight())); // JUCE kerning is a fraction of the font height
    g.setFont (f);
    g.setColour (c);
    Rectangle<float> box = r.expanded (30.0f, 0.0f); // room for font differences, text is never shortened
    if (j.testFlags (Justification::left))       box = r.withWidth (r.getWidth() + 60.0f);
    else if (j.testFlags (Justification::right)) box = r.withLeft (r.getX() - 60.0f);
    g.drawText (s, box, j, false);
}

// ---- Button in the mockup's .btn style -----------------------------------------------------------
class PButton : public Component, public TipClient
{
public:
    PButton (const Ctx& c, String t, float px = 14.0f, theme::FontKind k = theme::FontKind::regular)
        : text (std::move (t)), ctx (c), size (px), kind (k) { setMouseCursor (MouseCursor::PointingHandCursor); }

    std::function<void (const ModifierKeys&)> onClick;
    std::function<void()> onDouble;
    bool on = false;
    bool hot = false;                       // red border and pink text (Auto Trim has trimmed)
    float spacing = 0.5f;
    float radius = 4.0f;
    String text;

    void setText (const String& t) { if (t != text) { text = t; repaint(); } }
    void setOn (bool v) { if (v != on) { on = v; repaint(); } }

    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (on ? theme::sel : t.plate);
        g.fillRoundedRectangle (r, radius);
        Colour border = hot ? theme::red : on ? theme::selb : (over ? t.dim : t.line);
        g.setColour (border);
        g.drawRoundedRectangle (r, radius, 1.0f);
        const Colour tc = hot ? theme::pink : on ? theme::lilac : t.ink;
        auto f = theme::font (kind, size);
        if (text.contains ("\n"))
        {
            auto lines = StringArray::fromLines (text);
            const float lh = size * 1.15f;
            float y = r.getCentreY() - lh * (float) lines.size() * 0.5f;
            for (auto& l : lines) { drawText (g, l, { r.getX(), y, r.getWidth(), lh }, f, tc, Justification::centred, spacing); y += lh; }
        }
        else drawText (g, text, r, f, tc, Justification::centred, spacing);
    }
    void mouseEnter (const MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const MouseEvent&) override { over = false; repaint(); }
    void mouseUp (const MouseEvent& e) override
    {
        if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick) onClick (e.mods);
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onDouble) onDouble(); }

private:
    const Ctx& ctx;
    float size;
    theme::FontKind kind;
    bool over = false;
};

// ---- Faceplate knob, drawn like the mockup's SVG (150 x 150) ---------------------------------------
class KnobView : public Component, public TipClient
{
public:
    // ticks: the 11 labels printed around the dial (empty for no labels). small: the 60 px manual timing knobs.
    KnobView (RangedAudioParameter& prm, StringArray tickLabels, bool isSmall)
        : p (prm), ticks (std::move (tickLabels)), small (isSmall), att (prm, [this] (float) { repaint(); })
    {
        setMouseCursor (MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus (true);
        att.sendInitialUpdate();
    }

    double norm() const { return p.getValue(); }

    void paint (Graphics& g) override
    {
        if (small) g.addTransform (AffineTransform::scale ((float) getWidth() / 150.0f));
        const float cx = 75.0f, cy = 75.0f;
        for (int i = 0; i <= 20; ++i)
        {
            const float a = degreesToRadians (-135.0f + (float) i * 13.5f);
            const bool major = i % 2 == 0;
            const float r1 = major ? 66.0f : 68.0f, r2 = 72.0f;
            g.setColour (major ? Colour (0xfff4f5f7) : Colour (0xff8c8f99));
            g.drawLine (cx + std::sin (a) * r1, cy - std::cos (a) * r1, cx + std::sin (a) * r2, cy - std::cos (a) * r2, major ? 1.8f : 1.0f);
        }
        g.setColour (Colour (0xffc9cbd3));
        for (int i = 0; i < ticks.size() && i <= 10; ++i)
        {
            const float a = degreesToRadians (-135.0f + (float) i * 27.0f);
            g.setFont (theme::font (ticks[i].containsNonWhitespaceChars() && ticks[i].toWideCharPointer()[0] > 127 ? theme::FontKind::mono : theme::FontKind::semi, ticks[i].length() > 3 ? 8.5f : 10.0f));
            g.drawSingleLineText (ticks[i], roundToInt (cx + std::sin (a) * 57.0f), roundToInt (cy - std::cos (a) * 57.0f + 3.5f),
                                  Justification::horizontallyCentred);
        }
        g.setColour (Colour (0xff0b0b0d));
        g.fillEllipse (cx - 46, cy - 46, 92, 92);
        g.setColour (Colours::black);
        g.drawEllipse (cx - 46, cy - 46, 92, 92, 1.0f);
        {
            Path ring; ring.addEllipse (cx - 44, cy - 44, 88, 88);
            Path dashes; const float d[2] = { 1.4f, 1.6f };
            PathStrokeType (3.0f, PathStrokeType::mitered, PathStrokeType::butt).createDashedStroke (dashes, ring, d, 2);
            g.setColour (Colour (0xff3a3c43));
            g.fillPath (dashes);
        }
        const float ang = degreesToRadians (-135.0f + 270.0f * (float) norm());
        ColourGradient grad (Colour (0xfff1f1f4), 67.6f, 63.9f, Colour (0xff6e6f78), 67.6f + 51.8f, 63.9f, true);
        grad.addColour (0.55, Colour (0xffb9bac2));
        g.setGradientFill (grad);
        g.fillEllipse (cx - 37, cy - 37, 74, 74);
        g.setColour (Colour (0xff4a4b53));
        g.drawEllipse (cx - 37, cy - 37, 74, 74, 1.0f);
        g.setColour (Colours::black.withAlpha (0.25f));
        g.drawEllipse (cx - 33, cy - 33, 66, 66, 1.0f);
        g.setColour (Colour (0xff16171a));
        g.drawLine (cx, cy, cx + std::sin (ang) * 29.0f, cy - std::cos (ang) * 29.0f, 4.0f);
        g.fillEllipse (cx + std::sin (ang) * 29.0f - 2.0f, cy - std::cos (ang) * 29.0f - 2.0f, 4.0f, 4.0f);
        g.fillEllipse (cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
        if (hasKeyboardFocus (false))
        {
            g.setColour (theme::selb);
            g.drawEllipse (1.0f, 1.0f, 148.0f, 148.0f, 2.0f);
        }
    }

    void mouseDown (const MouseEvent&) override { startNorm = norm(); att.beginGesture(); grabKeyboardFocus(); }
    void mouseDrag (const MouseEvent& e) override
    {
        const double d = -(double) e.getDistanceFromDragStartY() / 250.0 * (e.mods.isShiftDown() ? 0.1 : 1.0);
        setNorm (startNorm + d);
    }
    void mouseUp (const MouseEvent&) override { att.endGesture(); }
    void mouseDoubleClick (const MouseEvent&) override { att.beginGesture(); setNorm (p.getDefaultValue()); att.endGesture(); }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        const float dy = w.isReversed ? -w.deltaY : w.deltaY;
        if (std::abs (dy) < 1.0e-6f) return;
        att.beginGesture();
        setNorm (norm() + (dy > 0 ? 1 : -1) * (e.mods.isShiftDown() ? 0.002 : 0.02));
        att.endGesture();
    }
    bool keyPressed (const KeyPress& k) override
    {
        const double s = k.getModifiers().isShiftDown() ? 0.002 : 0.01;
        double n = norm();
        if (k.isKeyCode (KeyPress::upKey) || k.isKeyCode (KeyPress::rightKey)) n += s;
        else if (k.isKeyCode (KeyPress::downKey) || k.isKeyCode (KeyPress::leftKey)) n -= s;
        else if (k.isKeyCode (KeyPress::homeKey)) n = 0.0;
        else if (k.isKeyCode (KeyPress::endKey)) n = 1.0;
        else return false;
        att.beginGesture(); setNorm (n); att.endGesture();
        return true;
    }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    void setNorm (double n)
    {
        n = jlimit (0.0, 1.0, n);
        att.setValueAsPartOfGesture (p.convertFrom0to1 ((float) n));
    }
    void setReal (float v) { att.beginGesture(); att.setValueAsPartOfGesture (v); att.endGesture(); }

private:
    RangedAudioParameter& p;
    StringArray ticks;
    bool small;
    ParameterAttachment att;
    double startNorm = 0.0;
};

// Value text under a knob: "5.0 · +15.0 dB". The Mix cell has clickable DRY and COMP ends.
class KnobValue : public Component, public TipClient
{
public:
    KnobValue (const Ctx& c) : ctx (c) {}
    String bold, rest;                       // bold part in ink, the rest dim
    bool mixCell = false;
    bool plainFont = false;                  // UI font instead of the mono font (Headroom note)
    std::function<void (float)> onEnd;       // Mix: 0 for DRY, 100 for COMP
    String dryTip, compTip;
    void setParts (const String& b, const String& r) { if (b != bold || r != rest) { bold = b; rest = r; repaint(); } }

    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto f = theme::font (plainFont ? theme::FontKind::regular : theme::FontKind::mono, 12.0f);
        auto r = getLocalBounds().toFloat();
        if (mixCell)
        {
            drawText (g, "DRY", r.removeFromLeft (30.0f), f, t.title, Justification::centredLeft, 1.0f);
            drawText (g, "COMP", r.removeFromRight (36.0f), f, t.title, Justification::centredRight, 1.0f);
            r = getLocalBounds().toFloat();
        }
        GlyphArrangement ga;
        const float wb = textWidth (f, bold), wr = textWidth (f, " " + rest);
        float x = r.getCentreX() - (wb + wr) * 0.5f;
        const float y = r.getCentreY() + f.getAscent() * 0.5f - 1.0f;
        g.setFont (f);
        g.setColour (t.ink);
        g.drawSingleLineText (bold, roundToInt (x), roundToInt (y));
        g.setColour (t.dim);
        g.drawSingleLineText (" " + rest, roundToInt (x + wb), roundToInt (y));
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (! mixCell || ! onEnd) return;
        if (e.x < 34) onEnd (0.0f);
        else if (e.x > getWidth() - 40) onEnd (100.0f);
    }
    String tipAt (Point<int> pt) const { return mixCell ? (pt.x < 34 ? dryTip : pt.x > getWidth() - 40 ? compTip : String()) : tip; }

private:
    const Ctx& ctx;
};

// Invisible click area over an LED and its label.
class HitArea : public Component, public TipClient
{
public:
    std::function<void (const ModifierKeys&)> onClick;
    HitArea() { setMouseCursor (MouseCursor::PointingHandCursor); }
    void mouseUp (const MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick (e.mods); }
};

} // namespace ui

#include "PluginEditor.h"
#include "Widgets.h"
#include "Views.h"
#include "Dialogs.h"
#include "Presets.h"

using namespace juce;
using namespace ui;

namespace
{
// ---- the wood cheeks: same three colours and vertical grain as PurrOne ---------------------------
struct Mul32
{
    uint32_t a;
    double next()
    {
        a += 0x6D2B79F5u;
        uint32_t t = a ^ (a >> 15);
        t *= (1u | a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
};

Image makeWood (uint32_t seed, bool edgeRight)
{
    const int w = 104, h = 1280;
    Image img (Image::ARGB, w, h, true);
    Graphics g (img);
    Mul32 r { seed };
    g.fillAll (Colour (0xff3d2210));
    for (int x = 0; x < w; ++x)
    {
        const double a = r.next();
        g.setColour (Colour::fromFloatRGBA (123 / 255.0f, 74 / 255.0f, 38 / 255.0f, (float) (0.40 * (0.4 + 0.6 * a))));
        g.fillRect (x, 0, 1, h);
        if (r.next() < 0.35)
        {
            const float al = (float) (0.75 * r.next());
            const double y0 = r.next() * h;
            const double len = h * (0.2 + 0.6 * r.next());
            g.setColour (Colour::fromFloatRGBA (90 / 255.0f, 52 / 255.0f, 25 / 255.0f, al));
            g.fillRect ((float) x, (float) y0, 1.0f, (float) len);
        }
        if (r.next() < 0.12)
        {
            g.setColour (Colours::black.withAlpha (0.35f));
            g.fillRect (x, 0, 1, h);
        }
    }
    g.setColour (Colours::black);
    g.fillRect (edgeRight ? 0 : w - 3, 0, 3, h);
    return img;
}

std::unique_ptr<Drawable> makeLogo()
{
    static const char* svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64"><path d="M8 6 L24 22 H40 L56 6 L60 36 Q60 58 32 60 Q4 58 4 36 Z" fill="#FF7BB1"/><path d="M12 14 L21 23 H14 Z M52 14 L43 23 H50 Z" fill="#FBBDD0"/><ellipse cx="22" cy="36" rx="4" ry="5" fill="#16171a"/><ellipse cx="42" cy="36" rx="4" ry="5" fill="#16171a"/><path d="M29 44 H35 L32 48 Z" fill="#16171a"/><path d="M32 48 Q28 53 24 50 M32 48 Q36 53 40 50" stroke="#16171a" stroke-width="2" fill="none"/><path d="M2 40 H14 M2 46 H14 M62 40 H50 M62 46 H50" stroke="#FBBDD0" stroke-width="1.6"/></svg>)";
    if (auto xml = XmlDocument::parse (svg)) return Drawable::createFromSVG (*xml);
    return {};
}

class Cheek : public Component, public TipClient
{
public:
    Cheek (Image i, std::function<void()> reset) : img (std::move (i)), onReset (std::move (reset)) {}
    void paint (Graphics& g) override
    {
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (img, getLocalBounds().toFloat());
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onReset) onReset(); }
private:
    Image img;
    std::function<void()> onReset;
};

struct Delay : Timer
{
    std::function<void()> fn;
    void start (int ms) { startTimer (ms); }
    void timerCallback() override { stopTimer(); if (fn) fn(); }
};

class Bubble : public Component
{
public:
    Bubble (const Ctx& c) : ctx (c) { setInterceptsMouseClicks (false, false); setVisible (false); }
    void showAt (const String& s, Rectangle<int> target, int parentW, int parentH)
    {
        text = s;
        AttributedString as = build();
        TextLayout tl; tl.createLayout (as, 282.0f);
        const int tw = (int) std::ceil (std::min (282.0f, tl.getWidth())) + 20, th = (int) std::ceil (tl.getHeight()) + 14;
        int x = jlimit (60, std::max (60, parentW - tw - 8), target.getX());
        int y = target.getBottom() + 6;
        if (y + th > parentH - 4) y = target.getY() - th - 6;
        setBounds (x, y, tw, th);
        setVisible (true);
        toFront (false);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        build().draw (g, r.reduced (10.0f, 7.0f));
    }
private:
    AttributedString build() const
    {
        AttributedString as; as.setWordWrap (AttributedString::byWord); as.setLineSpacing (2.0f);
        as.append (text, theme::font (theme::FontKind::regular, 14.0f), ctx.t().ink);
        return as;
    }
    const Ctx& ctx;
    String text;
};

class Toast : public Component
{
public:
    Toast (const Ctx&) { setInterceptsMouseClicks (false, false); setVisible (false); done.fn = [this] { setVisible (false); }; }
    void say (const String& s, int parentW, int parentH)
    {
        text = s;
        const int w = (int) textWidth (theme::font (theme::FontKind::regular, 15.0f), s) + 30;
        setBounds ((parentW - w) / 2, parentH - 34 - 28, w, 28);
        setVisible (true); toFront (false); repaint();
        done.start (1600);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        drawText (g, text, r, theme::font (theme::FontKind::regular, 15.0f), Colours::white);
    }
private:
    String text;
    Delay done;
};

} // namespace

// =====================================================================================================
class Face : public Component, public DialogHost, private Timer
{
public:
    Face (PunchPawProcessor& p, std::function<void()> resetSize)
        : proc (p), onResetSize (std::move (resetSize)), logo (makeLogo()),
          dial (ctx, live), curve (ctx, live), shape (ctx, live), hist (ctx, live), plate (ctx, live),
          meterIn (ctx), meterOut (ctx), bubble (ctx), toastView (ctx), modal (*this)
    {
        const String lk = proc.getSetting (setting::look, "C").toString();
        ctx.look = lk == "A" ? 'A' : lk == "B" ? 'B' : 'C';
        ctx.tips = (bool) proc.getSetting (setting::tips, true);
        ctx.power = (bool) proc.getSetting (setting::power, false);
        live.view = jlimit (0, 2, (int) proc.getSetting (setting::view, 0));
        setSize (layout::W, layout::H);
        setWantsKeyboardFocus (true);

        addAndMakeVisible (cheekL = new Cheek (makeWood (11, true), onResetSize));
        addAndMakeVisible (cheekR = new Cheek (makeWood (29, false), onResetSize));
        cheekL->setBounds (roundToInt (layout::cheekL.x), 0, roundToInt (layout::cheekL.w), layout::H);
        cheekR->setBounds (roundToInt (layout::cheekR.x), 0, roundToInt (layout::cheekR.w), layout::H);
        cheekL->tip = cheekR->tip = layout::cheekTip;

        buildHeader();
        buildSub();
        buildPlate();
        buildDisplays();
        buildKnobs();
        buildMeters();

        addAndMakeVisible (bubble);
        addAndMakeVisible (toastView);
        addChildComponent (modal);
        addMouseListener (this, true);
        tipTimer.fn = [this] { showTip(); };
        refreshLive (1.0f / 60.0f);
        syncControls();
        startTimerHz (ctx.power ? 16 : 60);
    }
    ~Face() override { stopTimer(); removeMouseListener (this); }
    void openForTesting (const String& n)
    {
        if (n == "settings") modal.openSettings();
        else if (n == "guide") modal.openGuide();
        else if (n == "about") modal.openAbout();
        else if (n == "presets") modal.openBrowser();
        else if (n == "save") modal.openSave (nullptr);
        else if (n == "export") modal.openExport (nullptr);
        else if (n == "import") modal.openImport();
        else if (n == "tip") { tipHost = &meterIn; tipText = "Info bubble check."; showTip(); }
        else if (n == "manual") { setReal (ids::timing, 1.0f); syncControls(); }
    }
    void setViewForTesting (int v) { setView (v); }
    void forceRunForTesting() { forceRun = true; }
    bool forceRun = false;

    // ---- DialogHost --------------------------------------------------------------------------
    PunchPawProcessor& processor() override { return proc; }
    presets::Store& store() override { return presetStore; }
    Ctx& context() override { return ctx; }
    String presetName() override { return proc.getSetting (setting::preset, presets::factory()[0].name).toString(); }
    void setPresetName (const String& n) override { proc.setSetting (setting::preset, n); syncControls(); }
    void toast (const String& s) override { toastView.say (s, getWidth(), getHeight()); }
    void resetWindow() override { if (onResetSize) onResetSize(); }
    void settingsChanged() override
    {
        startTimerHz (ctx.power ? 16 : 60);
        repaint();
        for (auto* c : getChildren()) c->repaint();
    }

    Preset currentValues() override
    {
        Preset p;
        p.v[0] = (real (ids::thr) + 40.0) / 4.0;
        p.v[1] = real (ids::ratio);
        p.v[2] = real (ids::out) / 4.0 + 5.0;
        p.v[3] = real (ids::hp);
        p.v[4] = real (ids::mix);
        p.v[5] = real (ids::ceil);
        p.v[6] = real (ids::knee);
        p.v[7] = real (ids::bypass);
        p.v[8] = real (ids::timing);
        p.v[9] = pp::atkKnob (real (ids::atk));
        p.v[10] = pp::relKnob (real (ids::rel));
        for (int i = 0; i < 11; ++i) if (i != 4 && i != 6 && i != 7 && i != 8) p.v[i] = std::round (p.v[i] * 1000.0) / 1000.0;
        return p;
    }

    void loadPreset (const Preset& p) override
    {
        setReal (ids::thr, pp::thrDbOf ((float) p.v[0]));
        setReal (ids::ratio, (float) p.v[1]);
        setReal (ids::out, pp::outDbOf ((float) p.v[2]));
        setReal (ids::hp, (float) p.v[3]);
        setReal (ids::mix, (float) p.v[4]);
        setReal (ids::ceil, (float) p.v[5]);
        setReal (ids::knee, (float) p.v[6]);
        setReal (ids::bypass, (float) p.v[7]);
        setReal (ids::timing, (float) p.v[8]);
        setReal (ids::atk, pp::atkMsOf ((float) p.v[9]));
        setReal (ids::rel, pp::relMsOf ((float) p.v[10]));
        setPresetName (p.name);
        toast ("Loaded " + p.name);
        refreshLive (1.0f / 60.0f);
    }

    bool dirty() override
    {
        const Preset* base = presetStore.find (presetName());
        if (base == nullptr) base = &presets::factory()[0];
        const Preset now = currentValues();
        for (int i = 0; i < presets::kCount; ++i)
        {
            if (i == 7) continue; // bypass is not part of a preset
            if (std::abs (now.v[i] - base->v[i]) > (i == 4 ? 0.2 : i == 6 || i == 8 ? 0.5 : 0.02)) return true;
        }
        return false;
    }

    // ---- painting ----------------------------------------------------------------------------
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        g.fillAll (Colours::black);
        g.setColour (t.plate);
        g.fillRect (rc (layout::body).withTop (0).withHeight ((float) layout::H));
        // header
        g.setColour (t.plate2); g.fillRect (rc (layout::hdr));
        g.setColour (t.line); g.fillRect (layout::hdr.x, layout::hdr.h - 1.0f, layout::hdr.w, 1.0f);
        if (logo) logo->drawWithin (g, rc (layout::logo), RectanglePlacement::centred, 1.0f);
        {
            const auto f = theme::font (theme::FontKind::brand, 26.0f);
            const float sp = 0.5f, wp = textWidth (f, "Punch") + sp * 5.0f, wa = textWidth (f, "Paw") + sp * 3.0f;
            const float x0 = layout::name.x + (layout::name.w - wp - wa) * 0.5f;
            drawText (g, "Punch", { x0, layout::name.y, wp, layout::name.h }, f, Colours::white, Justification::centredLeft, sp);
            drawText (g, "Paw", { x0 + wp, layout::name.y, wa, layout::name.h }, f, t.accent, Justification::centredLeft, sp);
        }
        drawText (g, "STUDIO 87", rc (layout::s87), theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centred, 2.0f);
        // sub strip
        g.setColour (t.plate); g.fillRect (rc (layout::sub));
        g.setColour (t.line); g.fillRect (layout::sub.x, layout::sub.y + layout::sub.h - 1.0f, layout::sub.w, 1.0f);
        drawText (g, layout::subText.s, rc (layout::subText.r), theme::font (theme::FontKind::regular, layout::subText.size), t.ink, Justification::centred, layout::subText.spacing);
        drawText (g, String (layout::subBest.s).toUpperCase(), rc (layout::subBest.r), theme::font (theme::FontKind::regular, layout::subBest.size), t.dim, Justification::centred, layout::subBest.spacing);
        for (int i = 0; i < 5; ++i)
        {
            auto& tg = layout::subTags[i];
            const bool no = i == 4;
            auto r = rc (tg.r).reduced (0.5f);
            g.setColour (no ? theme::pink : t.accent);
            g.drawRoundedRectangle (r, 9.0f, 1.0f);
            drawText (g, tg.s, rc (tg.r), theme::font (theme::FontKind::regular, tg.size), no ? theme::pink : t.title, Justification::centred, tg.spacing);
        }
        // top plate bottom border, display strip
        g.setColour (t.line); g.fillRect (layout::top.x, layout::top.y + layout::top.h - 1.0f, layout::top.w, 1.0f);
        g.setColour (t.plate2); g.fillRect (rc (layout::disp));
        g.setColour (t.line); g.fillRect (layout::disp.x, layout::disp.y + layout::disp.h - 1.0f, layout::disp.w, 1.0f);
        const auto capFont = theme::font (theme::FontKind::regular, 12.0f);
        for (int i = 0; i < 3; ++i)
        {
            if (i == 1)
            {
                drawText (g, "PUNCH SHAPE, LAST 1.2 S:", rc (layout::dispCaps[1]), capFont, t.title, Justification::centredLeft, 1.5f);
                const Colour cols[3] = { t.dim, t.accent, theme::pink };
                for (int k = 0; k < 3; ++k)
                    drawText (g, String (layout::shapePartText[k]).toUpperCase(), rc (layout::shapeParts[k]), capFont, cols[k], Justification::centredLeft, 1.5f);
            }
            else drawText (g, String (layout::dispCapText[i]).toUpperCase(), rc (layout::dispCaps[i]), capFont, t.title, Justification::centredLeft, 1.5f);
        }
        // knob labels and the "added by" divider
        for (int i = 0; i < 6; ++i)
            drawText (g, String (layout::cells[i].label).toUpperCase(), rc (layout::cells[i].labelR), theme::font (theme::FontKind::bold, 15.0f), t.title, Justification::centred, 3.0f);
        if (live.p.manual)
            for (int i = 0; i < 2; ++i)
            {
                drawText (g, String (layout::smallLabel[i]).toUpperCase(), rc (layout::smallKnobs[i].label), theme::font (theme::FontKind::bold, 13.0f), t.title, Justification::centred, 2.0f);
                for (int e = 0; e < 2; ++e)
                    drawText (g, e == 0 ? layout::smallKnobs[i].e0 : layout::smallKnobs[i].e1, rc (e == 0 ? layout::smallKnobs[i].end0 : layout::smallKnobs[i].end1),
                              theme::font (theme::FontKind::mono, 9.0f), t.dim, Justification::centred);
            }
        {
            auto r = rc (layout::vdiv.r);
            g.setColour (t.line); g.fillRect (r.getX(), r.getY(), 1.0f, r.getHeight());
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, r.getCentreX() + 2.0f, r.getCentreY()));
            drawText (g, String (layout::vdiv.s).toUpperCase(), { r.getCentreX() + 2.0f - r.getHeight() * 0.5f, r.getCentreY() - r.getWidth() * 0.5f, r.getHeight(), r.getWidth() },
                      theme::font (theme::FontKind::regular, 10.0f), t.dim, Justification::centred, 1.5f);
        }
        // meter column
        g.setColour (t.plate2); g.fillRect (rc (layout::meters));
        g.setColour (t.line); g.fillRect (layout::meters.x, layout::meters.y, 1.0f, layout::meters.h);
        drawText (g, "IN", rc (layout::meterCaps[0]), capFont, t.title, Justification::centred, 1.5f);
        drawText (g, "OUT", rc (layout::meterCaps[1]), capFont, t.title, Justification::centred, 1.5f);
        auto pk = [] (float lin) { return lin > 1.0e-4f ? String (l2d (lin), 1) : String ("-inf"); };
        {   // the two readouts sit side by side with a 6 px gap, centred under the strips
            const auto f = theme::font (theme::FontKind::mono, 11.0f);
            const String a = pk (live.inPk), b = pk (live.outPk);
            const float wa = textWidth (f, a), wb = textWidth (f, b);
            const float cx = (layout::pkIn.x + layout::pkOut.x + layout::pkOut.w) * 0.5f;
            const float x0 = cx - (wa + 6.0f + wb) * 0.5f;
            drawText (g, a, { x0, layout::pkIn.y, wa, layout::pkIn.h }, f, t.dim);
            drawText (g, b, { x0 + wa + 6.0f, layout::pkIn.y, wb, layout::pkIn.h }, f, t.dim);
        }
        // footer
        g.setColour (t.plate2); g.fillRect (rc (layout::foot));
        g.setColour (t.line); g.fillRect (layout::foot.x, layout::foot.y, layout::foot.w, 1.0f);
        drawText (g, String ("Studio 87 ") + String::fromUTF8 ("\xc2\xb7") + " PunchPaw v" + JucePlugin_VersionString,
                  { layout::foot.x + 12.0f, layout::foot.y, 400.0f, layout::foot.h }, theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centredLeft, 1.0f);
        if (auto* ed = findParentComponentOfClass<PunchPawEditor>())
            drawText (g, String (ed->getWidth()) + String::fromUTF8 (" \xc3\x97 ") + String (ed->getHeight()) + " px",
                      { layout::foot.x + layout::foot.w - 12.0f - 120.0f, layout::foot.y, 120.0f, layout::foot.h },
                      theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centredRight, 1.0f);
    }

    // ---- keyboard ----------------------------------------------------------------------------
    bool keyPressed (const KeyPress& k) override
    {
        if (modal.isOpen() || k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown() || k.getModifiers().isAltDown()) return false;
        const juce_wchar c = CharacterFunctions::toLowerCase (k.getTextCharacter());
        if (c == 'b') toggle (ids::bypass);
        else if (c == 'v') setView ((live.view + 1) % 3);
        else if (c == 'k') toggle (ids::knee);
        else if (c == 't') toggle (ids::timing);
        else return false;
        return true;
    }

    // ---- info bubbles ------------------------------------------------------------------------
    void mouseMove (const MouseEvent& e) override { trackTip (e); }
    void mouseEnter (const MouseEvent& e) override { trackTip (e); }
    void mouseExit (const MouseEvent&) override { hideTip(); }
    void mouseDown (const MouseEvent& e) override
    {
        hideTip();
        if (dynamic_cast<TextEditor*> (e.eventComponent) == nullptr && ! modal.isOpen()) grabKeyboardFocus();
    }

private:
    void timerCallback() override
    {
        if (! forceRun && ! isShowing()) return;                 // pause drawing when the window is hidden; audio is unaffected
        refreshLive (1.0f / (float) (ctx.power ? 16 : 60));
        dial.repaint(); curve.repaint(); shape.repaint(); hist.repaint();
        meterIn.update (live.inPk); meterOut.update (live.outPk);
        plate.repaint();
        repaint (Rectangle<int> (1056, 394, 92, 20));
        syncControls();
    }

    float real (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setReal (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
    void toggle (const char* id) { setReal (id, real (id) > 0.5f ? 0.0f : 1.0f); syncControls(); }
    void setView (int v) { live.view = v; proc.setSetting (setting::view, v); plate.repaint(); dial.repaint(); }

    void refreshLive (float dt)
    {
        live.fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
        const float dec = std::exp (-dt / 0.1f);
        live.inPk = std::max (proc.inPeak.exchange (0.0f), live.inPk * dec);
        live.outPk = std::max (proc.outPeak.exchange (0.0f), live.outPk * dec);
        live.grNow = proc.grNow.load();
        live.vuI = proc.vuIn.load(); live.vuO = proc.vuOut.load();
        live.over = proc.overDb.load(); live.atkNow = proc.atkNow.load();
        const float gp = proc.grPeak.exchange (0.0f);
        const int reps = ctx.power ? 4 : 1;                     // keep the six second span at 16 frames per second
        for (int r = 0; r < reps; ++r) { live.hist[live.hi] = gp; live.hi = (live.hi + 1) % 360; }
        live.p = PunchPawProcessor::toCore (real (ids::thr), real (ids::ratio), real (ids::out), real (ids::hp), real (ids::mix), real (ids::ceil),
                                            real (ids::knee) > 0.5f, real (ids::timing) > 0.5f, real (ids::atk), real (ids::rel));
        live.bypass = real (ids::bypass) > 0.5f;
        const auto& c = proc.core();
        live.scI = c.scopeIdx.load (std::memory_order_acquire);
        for (int i = 0; i < pp::kScopeN; ++i)
        {
            live.scIn[i] = c.scIn[(size_t) i].load (std::memory_order_relaxed);
            live.scOut[i] = c.scOut[(size_t) i].load (std::memory_order_relaxed);
            live.scGr[i] = c.scGr[(size_t) i].load (std::memory_order_relaxed);
        }
    }

    static String msText (float ms)
    {
        if (ms >= 1000.0f) return String (ms / 1000.0f, 2) + " s";
        if (ms >= 100.0f) return String (roundToInt (ms)) + " ms";
        return String (ms, ms >= 10.0f ? 1 : 2) + " ms";
    }

    void syncControls()
    {
        bBypass->setOn (live.bypass);
        const bool man = live.p.manual;
        if ((int) man != lastManual)
        {
            lastManual = man ? 1 : 0;
            for (int i = 0; i < 2; ++i) { smallKnobs[i]->setVisible (man); smallValues[i]->setVisible (man); }
            for (auto* a : autoOnly) a->setVisible (! man);
            for (auto* a : manualOnly) a->setVisible (man);
            repaint();
        }
        const float thr = real (ids::thr), out = real (ids::out), hp = pp::hpHzOf (real (ids::hp)), ceilDb = pp::ceilDbOf (real (ids::ceil));
        values[0]->setParts (String (thr, 1), "dBFS");
        values[1]->setParts (String (pp::ratioText (real (ids::ratio))), {});
        values[2]->setParts ((out >= 0 ? "+" : "") + String (out, 1), "dB");
        values[3]->setParts (hp <= 0.0f ? String ("Off") : String (hp < 100.0f ? roundToInt (hp) : roundToInt (hp)), hp <= 0.0f ? String() : String ("Hz"));
        values[4]->setParts (String (roundToInt (real (ids::mix))) + "%", "comp");
        values[5]->setParts (ceilDb >= 50.0f ? String ("Off") : String (ceilDb, 1), ceilDb >= 50.0f ? String() : String ("dBFS"));
        smallValues[0]->setParts (msText (real (ids::atk)), {});
        smallValues[1]->setParts (msText (real (ids::rel)), {});
        pName->setText (presetName() + (dirty() ? " *" : ""));
    }

    // ---- building ----------------------------------------------------------------------------
    PButton* addButton (Component& parent, const String& text, const layout::Rc& r, const String& tip, float px = 14.0f, theme::FontKind k = theme::FontKind::regular)
    {
        auto* b = new PButton (ctx, text, px, k);
        owned.add (b);
        parent.addAndMakeVisible (b);
        b->setBounds (roundToInt (r.x), roundToInt (r.y), roundToInt (r.w), roundToInt (r.h));
        b->tip = tip;
        return b;
    }

    void buildHeader()
    {
        pPrev = addButton (*this, String::fromUTF8 ("\xe2\x97\x80"), layout::pPrev, layout::headerTips[0]);
        pName = addButton (*this, presetName(), layout::pName, layout::headerTips[1], 16.0f, theme::FontKind::semi);
        pNext = addButton (*this, String::fromUTF8 ("\xe2\x96\xb6"), layout::pNext, layout::headerTips[2]);
        pSave = addButton (*this, "Save", layout::pSave, layout::headerTips[3]);
        bBypass = addButton (*this, "Bypass", layout::bBypass, layout::headerTips[4]);
        bGuide = addButton (*this, "Guide", layout::bGuide, layout::headerTips[5]);
        bSettings = addButton (*this, "Settings", layout::bSettings, layout::headerTips[6]);
        bAbout = addButton (*this, "About & Support", layout::bAbout, layout::headerTips[7]);
        pPrev->onClick = [this] (const ModifierKeys&) { step (-1); };
        pNext->onClick = [this] (const ModifierKeys&) { step (1); };
        pSave->onClick = [this] (const ModifierKeys&) { modal.openSave (nullptr); };
        bBypass->onClick = [this] (const ModifierKeys&) { toggle (ids::bypass); };
        bGuide->onClick = [this] (const ModifierKeys&) { modal.openGuide(); };
        bSettings->onClick = [this] (const ModifierKeys&) { modal.openSettings(); };
        bAbout->onClick = [this] (const ModifierKeys&) { modal.openAbout(); };
        pName->onClick = [this] (const ModifierKeys&) { nameDelay.start (260); };
        nameDelay.fn = [this] { modal.openBrowser(); };
        pName->onDouble = [this]
        {
            nameDelay.stopTimer();
            const Preset* p = presetStore.find (presetName());
            if (p == nullptr || p->factory) { toast ("Save a copy first, factory presets cannot be renamed"); return; }
            Preset q = *p; modal.openSave (&q);
        };
    }

    void step (int d)
    {
        auto all = presetStore.all();
        int i = 0;
        for (size_t k = 0; k < all.size(); ++k) if (all[k].name == presetName()) i = (int) k;
        i = (i + d + (int) all.size()) % (int) all.size();
        loadPreset (all[(size_t) i]);
    }

    HitArea* addHit (Component& parent, const layout::Rc& r, const String& tip, std::function<void (const ModifierKeys&)> fn = {}, float dx = 0, float dy = 0)
    {
        auto* a = new HitArea(); owned.add (a);
        parent.addAndMakeVisible (a);
        a->setBounds (roundToInt (r.x - dx), roundToInt (r.y - dy), roundToInt (r.w), roundToInt (r.h));
        a->tip = tip; a->onClick = std::move (fn);
        if (! a->onClick) a->setMouseCursor (MouseCursor::NormalCursor);
        return a;
    }

    void buildSub()
    {
        addHit (*this, layout::sub, layout::subTip);
    }

    void buildPlate()
    {
        plate.setBounds (roundToInt (layout::top.x), roundToInt (layout::top.y), roundToInt (layout::top.w), roundToInt (layout::top.h));
        addAndMakeVisible (plate);
        const float ox = layout::top.x, oy = layout::top.y;
        for (int i = 0; i < 3; ++i) addHit (plate, layout::viewHits[i].r, layout::viewHits[i].tip, [this, i] (const ModifierKeys&) { setView (i); }, ox, oy);
        for (int i = 0; i < 2; ++i) addHit (plate, layout::timingHits[i].r, layout::timingHits[i].tip, [this, i] (const ModifierKeys&) { setReal (ids::timing, (float) i); syncControls(); }, ox, oy);
        for (int i = 0; i < 2; ++i) addHit (plate, layout::kneeHits[i].r, layout::kneeHits[i].tip, [this, i] (const ModifierKeys&) { setReal (ids::knee, (float) i); syncControls(); }, ox, oy);
        for (int i = 0; i < 12; ++i)
        {
            const auto& l = layout::ladder[i];
            addHit (plate, { l.cx - 12, l.cy - 12, 24, 24 }, layout::ladderTips[i], {}, ox, oy);
        }
        for (int i = 0; i < 3; ++i)
        {
            const auto& l = layout::thrLeds[i];
            addHit (plate, { l.cx - 8, l.cy - 12, 70, 24 }, layout::thrTips[i], {}, ox, oy);
        }
        for (int i = 0; i < 2; ++i) autoOnly.add (addHit (plate, layout::autoRead[i].r, layout::autoRead[i].tip, {}, ox, oy));
        for (int i = 0; i < 31; ++i)
        {
            auto& x = layout::plateTexts[i];
            if (String (x.tip).isNotEmpty() && String (x.s).isNotEmpty())
            {
                auto* h = addHit (plate, x.r, x.tip, {}, ox, oy);
                if (i >= layout::autoNoteFirst && i < layout::autoNoteFirst + 2) autoOnly.add (h);
            }
        }
    }

    void buildDisplays()
    {
        CanvasView* views[4] = { &dial, &curve, &shape, &hist };
        const layout::Rc* rects[4] = { &layout::cvDial, &layout::cvCurve, &layout::cvShape, &layout::cvHist };
        const char* tips[4] = { layout::dialTip, layout::displayTips[0], layout::displayTips[1], layout::displayTips[2] };
        for (int i = 0; i < 4; ++i)
        {
            addAndMakeVisible (*views[i]);
            views[i]->setBounds (roundToInt (rects[i]->x), roundToInt (rects[i]->y), roundToInt (rects[i]->w), roundToInt (rects[i]->h));
            views[i]->tip = tips[i];
        }
    }

    void buildKnobs()
    {
        const char* pid[6] = { ids::thr, ids::ratio, ids::out, ids::hp, ids::mix, ids::ceil };
        for (int i = 0; i < 6; ++i)
        {
            const auto& c = layout::cells[i];
            StringArray ticks;
            for (int k = 0; k < c.nticks; ++k) ticks.add (c.ticks[k]);
            auto* k = new KnobView (*proc.apvts.getParameter (pid[i]), ticks, false);
            knobs.add (k); addAndMakeVisible (k);
            k->setBounds (roundToInt (c.knob.x), roundToInt (c.knob.y), 150, 150);
            k->tip = c.tip;
            auto* v = new KnobValue (ctx);
            values.add (v); addAndMakeVisible (v);
            { const int pad = (i == 4) ? 0 : 40; v->setBounds (roundToInt (c.value.x) - pad, roundToInt (c.value.y), roundToInt (c.value.w) + 2 * pad, roundToInt (c.value.h)); }
            v->tip = c.tip;
            if (i == 4)
            {
                v->mixCell = true;
                v->dryTip = "Click DRY to set the knob fully dry.";
                v->compTip = "Click COMP to set the knob fully processed.";
                v->tip = {};
                auto* kk = k;
                v->onEnd = [kk] (float pct) { kk->setReal (pct); };
            }
        }
        addHit (*this, layout::vdiv.r, layout::vdiv.tip);
        const char* sid[2] = { ids::atk, ids::rel };
        for (int i = 0; i < 2; ++i)
        {
            const auto& c = layout::smallKnobs[i];
            auto* k = new KnobView (*proc.apvts.getParameter (sid[i]), StringArray(), true);
            smallKnobs.add (k); addChildComponent (k);
            k->setBounds (roundToInt (c.knob.x), roundToInt (c.knob.y), 60, 60);
            k->tip = c.tip;
            auto* v = new KnobValue (ctx);
            smallValues.add (v); addChildComponent (v);
            v->setBounds (roundToInt (c.val.x) - 30, roundToInt (c.val.y), roundToInt (c.val.w) + 60, roundToInt (c.val.h));
            v->tip = c.tip;
            for (int e = 0; e < 2; ++e) manualOnly.add (addHit (*this, e == 0 ? c.end0 : c.end1, c.tip));
            for (int e = 0; e < 2; ++e) manualOnly.getLast()->setVisible (false);
        }
        for (auto* m : manualOnly) m->setVisible (false);
    }

    void buildMeters()
    {
        addAndMakeVisible (meterIn); addAndMakeVisible (meterOut);
        meterIn.setBounds (roundToInt (layout::cvIn.x), roundToInt (layout::cvIn.y), roundToInt (layout::cvIn.w), roundToInt (layout::cvIn.h));
        meterOut.setBounds (roundToInt (layout::cvOut.x), roundToInt (layout::cvOut.y), roundToInt (layout::cvOut.w), roundToInt (layout::cvOut.h));
        meterIn.tip = layout::meterTips[0]; meterOut.tip = layout::meterTips[1];
        addHit (*this, { layout::pkIn.x - 4, layout::pkIn.y - 2, layout::pkIn.w + 8, layout::pkIn.h + 4 }, layout::meterTips[2]);
        addHit (*this, { layout::pkOut.x - 4, layout::pkOut.y - 2, layout::pkOut.w + 8, layout::pkOut.h + 4 }, layout::meterTips[3]);
    }

    // ---- tips --------------------------------------------------------------------------------
    void trackTip (const MouseEvent& e)
    {
        if (modal.isOpen() || ! ctx.tips) { hideTip(); return; }
        Component* c = e.eventComponent;
        String text; Component* host = nullptr;
        while (c != nullptr && c != this)
        {
            if (auto* kv = dynamic_cast<KnobValue*> (c))
            {
                text = kv->tipAt (e.getEventRelativeTo (c).getPosition());
                if (text.isEmpty()) text = kv->tip;
                host = c; if (text.isNotEmpty()) break;
            }
            else if (auto* tc = dynamic_cast<TipClient*> (c); tc != nullptr && tc->tip.isNotEmpty())
            {
                text = tc->tip; host = c; break;
            }
            c = c->getParentComponent();
        }
        if (host == nullptr || text.isEmpty()) { hideTip(); return; }
        if (host == tipHost && text == tipText) return;
        hideTip();
        tipHost = host; tipText = text;
        tipTimer.start (1000);
    }
    void showTip()
    {
        if (tipHost == nullptr || modal.isOpen()) return;
        bubble.showAt (tipText, getLocalArea (tipHost, tipHost->getLocalBounds()), getWidth(), getHeight());
    }
    void hideTip() { tipTimer.stopTimer(); bubble.setVisible (false); tipHost = nullptr; tipText = {}; }

    // ---- members -----------------------------------------------------------------------------
    PunchPawProcessor& proc;
    std::function<void()> onResetSize;
    Ctx ctx;
    Live live;
    presets::Store presetStore;
    std::unique_ptr<Drawable> logo;
    Cheek* cheekL = nullptr; Cheek* cheekR = nullptr;
    OwnedArray<Component> owned;
    PButton *pPrev = nullptr, *pName = nullptr, *pNext = nullptr, *pSave = nullptr, *bBypass = nullptr, *bGuide = nullptr, *bSettings = nullptr, *bAbout = nullptr;
    DialView dial; CurveView curve; ShapeView shape; HistView hist;
    PlateView plate;
    OwnedArray<KnobView> knobs, smallKnobs;
    OwnedArray<KnobValue> values, smallValues;
    Array<Component*> autoOnly, manualOnly;
    int lastManual = -1;
    MeterView meterIn, meterOut;
    Bubble bubble; Toast toastView;
    Modal modal;
    Delay nameDelay, tipTimer;
    Component* tipHost = nullptr;
    String tipText;
};

// =====================================================================================================
PunchPawEditor::PunchPawEditor (PunchPawProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    face = std::make_unique<Face> (p, [this] { resetSize(); });
    addAndMakeVisible (*face);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    constrainer.setMinimumSize (480, 228);
    constrainer.setMaximumSize (1800, 855);
    setConstrainer (&constrainer);
    setResizable (true, false);
    border = std::make_unique<ResizableBorderComponent> (this, &constrainer);
    border->setBorderThickness (BorderSize<int> (5));
    addAndMakeVisible (*border);
    const double sc = jlimit (0.4, 1.5, (double) proc.getSetting (setting::scale, 1.0));
    setSize (roundToInt (kW * sc), roundToInt (kH * sc));
}

PunchPawEditor::~PunchPawEditor() = default;

void PunchPawEditor::resized()
{
    const double s = (double) getWidth() / (double) kW;
    face->setTransform (AffineTransform::scale ((float) s));
    face->setBounds (0, 0, kW, kH);
    border->setBounds (getLocalBounds());
    proc.setSetting (setting::scale, s);
}

void PunchPawEditor::resetSize() { setSize (kW, kH); }
void PunchPawEditor::openDialogForTesting (const juce::String& n) { face->openForTesting (n); }
void PunchPawEditor::setViewForTesting (int v) { face->setViewForTesting (v); }
void PunchPawEditor::forceRunForTesting() { face->forceRunForTesting(); }

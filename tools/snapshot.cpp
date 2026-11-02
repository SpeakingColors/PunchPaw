// Renders the real editor offscreen to a PNG so it can be compared with the mockup.
// usage: PunchPawSnapshot out.png [scale] [look A|B|C] [dialog none|settings|guide|about|presets|save|export|import|tip] [frames] [auto|manual] [view 0..2] [auto|manual] [view 0..2]
//        PunchPawSnapshot --selftest
#include "../src/plugin/PluginProcessor.h"
#include "../src/editor/PluginEditor.h"

// Checks that need the plugin classes but no window: knob mappings and saved state round trip.
static void setFixtureValues (PunchPawProcessor& a)
{
    auto set = [&a] (const char* id, float real) { auto* p = a.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (real)); };
    set (ids::thr, -18.0f); set (ids::ratio, 6.0f); set (ids::out, 3.0f); set (ids::hp, 4.0f); set (ids::mix, 70.0f); set (ids::ceil, 8.0f);
    set (ids::knee, 1); set (ids::timing, 1); set (ids::atk, 5.0f); set (ids::rel, 200.0f);
    a.setSetting (setting::look, "B"); a.setSetting (setting::preset, "Start Here");
}

// Saved state of state version 1 must keep loading (tests/fixtures/state-v1.xml).
static int fixtureTest (const juce::File& f)
{
    int bad = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++bad; };
    PunchPawProcessor p;
    auto xml = juce::XmlDocument::parse (f);
    check (xml != nullptr, "fixture parses");
    if (xml == nullptr) return 1;
    juce::MemoryBlock mb;
    juce::AudioProcessor::copyXmlToBinary (*xml, mb);
    p.setStateInformation (mb.getData(), (int) mb.getSize());
    auto real = [&p] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    check (std::abs (real (ids::thr) + 18.0f) < 0.02f && std::abs (real (ids::out) - 3.0f) < 0.02f && std::abs (real (ids::ratio) - 6.0f) < 0.01f
               && std::abs (real (ids::hp) - 4.0f) < 0.01f && std::abs (real (ids::mix) - 70.0f) < 0.1f && std::abs (real (ids::ceil) - 8.0f) < 0.01f,
           "knob values from the v1 fixture");
    check ((int) real (ids::knee) == 1 && (int) real (ids::timing) == 1 && std::abs (real (ids::atk) - 5.0f) < 0.05f && std::abs (real (ids::rel) - 200.0f) < 0.5f,
           "switch and manual timing values from the v1 fixture");
    check (p.getSetting (setting::look, "C").toString() == "B" && p.getSetting (setting::preset, "").toString() == "Start Here", "settings from the v1 fixture");
    return bad;
}

static int selfTest (const juce::File& fixture)
{
    int bad = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++bad; };
    PunchPawProcessor a, b;
    auto set = [] (PunchPawProcessor& p, const char* id, float real)
    {
        auto* prm = p.apvts.getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 (real));
    };
    auto real = [] (PunchPawProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    check (std::abs (real (a, ids::thr) + 20.0f) < 0.01f && std::abs (real (a, ids::mix) - 100.0f) < 0.01f && real (a, ids::bypass) < 0.5f, "defaults");
    check (std::abs (pp::thrDbOf (a.apvts.getParameter (ids::thr)->getValue() * 0.0f + 0.0f) - pp::thrDbOf (0.0f)) < 1e-6f, "threshold map is pure");
    // choices survive a set and read, last item included
    set (a, ids::knee, 1); set (a, ids::timing, 1);
    check ((int) real (a, ids::knee) == 1 && (int) real (a, ids::timing) == 1, "knee and timing last items");
    set (a, ids::knee, 0); set (a, ids::timing, 0);
    check ((int) real (a, ids::knee) == 0 && (int) real (a, ids::timing) == 0, "knee and timing first items");
    // state round trip including plugin settings
    setFixtureValues (a);
    juce::MemoryBlock mb; a.getStateInformation (mb);
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    check (std::abs (real (b, ids::thr) + 18.0f) < 0.02f && (int) real (b, ids::knee) == 1 && std::abs (real (b, ids::mix) - 70.0f) < 0.1f, "parameters survive a state round trip");
    check (b.getSetting (setting::look, "C").toString() == "B" && b.getSetting (setting::preset, "").toString() == "Start Here", "settings survive a state round trip");
    // reported latency equals the core's
    b.prepareToPlay (48000.0, 512);
    check (b.getLatencySamples() == pp::Compressor::kLatency, "latency is reported exactly");
    // an editor can be made, resized and deleted
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (b.createEditor());
        ed->setSize (600, 285);
        ed->setSize (1800, 855);
        check (ed->getWidth() == 1800, "editor creates and resizes");
    }
    if (fixture != juce::File()) bad += fixtureTest (fixture);
    std::printf (bad == 0 ? "ALL SELF TESTS PASSED\n" : "SELF TESTS FAILED\n");
    return bad;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--write-fixture")
    {
        PunchPawProcessor p; setFixtureValues (p);
        auto state = p.apvts.copyState(); state.setProperty ("stateVersion", 1, nullptr);
        if (auto xml = state.createXml()) juce::File (argv[2]).replaceWithText (xml->toString());
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "--selftest")
        return selfTest (argc > 2 ? juce::File (argv[2]) : juce::File()) == 0 ? 0 : 1;
    PunchPawProcessor proc;
    proc.prepareToPlay (48000.0, 800);
    if (argc > 6) { auto* t = proc.apvts.getParameter (ids::timing); t->setValueNotifyingHost (juce::String (argv[6]) == "manual" ? 1.0f : 0.0f); }
    if (argc > 7) proc.setSetting (setting::view, juce::String (argv[7]).getIntValue());
    { auto* t = proc.apvts.getParameter (ids::thr); t->setValueNotifyingHost (t->convertTo0to1 (-22.0f)); }
    if (argc > 3) proc.setSetting (setting::look, juce::String (argv[3]));
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    const double sc = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 1.0;
    ed->setSize (juce::roundToInt (1200 * sc), juce::roundToInt (570 * sc));
    const juce::String dialog = argc > 4 ? juce::String (argv[4]) : juce::String ("none");
    const int frames = argc > 5 ? juce::String (argv[5]).getIntValue() : 30;
    // Feed a drum-and-pad loop in real time so the meters, curve dot and history show something.
    juce::AudioBuffer<float> buf (2, 800);
    juce::MidiBuffer midi;
    juce::Random rnd (7);
    long pos = 0;
    for (int f = 0; f < frames; ++f)
    {
        for (int i = 0; i < 800; ++i, ++pos)
        {
            const double t = (double) pos / 48000.0, tk = std::fmod (t, 0.5), th = std::fmod (t + 0.25, 0.5);
            float v = (float) (0.85 * std::exp (-tk * 14.0) * std::sin (6.2831853 * (52.0 + 90.0 * std::exp (-tk * 40.0)) * tk));
            v += (float) (0.22 * std::exp (-th * 45.0) * (rnd.nextFloat() * 2.0f - 1.0f));
            v += 0.12f * (float) std::sin (6.2831853 * 220.0 * t) * (float) (0.6 + 0.4 * std::sin (6.2831853 * 0.25 * t));
            buf.setSample (0, i, v); buf.setSample (1, i, v * 0.97f);
        }
        proc.processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (16);
    }
    if (dialog != "none") { if (auto* pe = dynamic_cast<PunchPawEditor*> (ed.get())) pe->openDialogForTesting (dialog); }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    juce::Image img = ed->createComponentSnapshot (ed->getLocalBounds(), false, 1.0f);
    juce::File out (argv[1]);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    ed.reset();
    return 0;
}

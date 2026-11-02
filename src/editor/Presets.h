#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Presets use the faceplate units of the mockup: knobs 0..10, Mix 0..100, switches 0/1
// (threshold, ratio, output, SC high-pass, ceiling, attack and release are knob positions).
// Unknown keys are ignored, out of range values refuse the preset.
struct Preset
{
    juce::String name, type = "Bus";
    juce::StringArray tags;
    bool factory = false;
    // thr, ratio, out, hp, mix, ceil, knee, bypass, timing, atk, rel
    double v[11] { 5, 4, 5, 0, 100, 10, 0, 0, 0, 6.666666667, 5.110917 };
};

namespace presets
{
inline constexpr int kCount = 11;
inline const char* const keys[kCount] = { "thr", "ratio", "out", "hp", "mix", "ceil", "knee", "bypass", "timing", "atk", "rel" };
inline const double lo[kCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
inline const double hi[kCount] = { 10, 10, 10, 10, 100, 10, 1, 1, 1, 10, 10 };
inline const char* const types[] = { "Kick", "Snare", "Drum Bus", "Perc", "Bass", "Bus", "FX" };
inline constexpr int kNumTypes = 7;

const std::vector<Preset>& factory();
juce::File userFolder();

class Store
{
public:
    Store();
    void reload();                                    // read the user folder
    std::vector<Preset> all() const;                  // factory first, then yours
    const std::vector<Preset>& user() const { return mine; }
    const Preset* find (const juce::String& name) const;
    bool save (const Preset& p, const juce::String& replaceName = {}); // writes the file, replaces a same-named one
    bool remove (const juce::String& name);
    juce::String exportJson (const std::vector<Preset>& list) const;
    int importJson (const juce::String& text);        // returns count, or -1 when invalid
private:
    std::vector<Preset> mine;
};

bool valid (const juce::var& presetObject);
Preset fromVar (const juce::var& o);
juce::var toVar (const Preset& p);
} // namespace presets

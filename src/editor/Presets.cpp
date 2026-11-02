#include "Presets.h"
using namespace juce;

namespace presets
{
static Preset mk (const char* name, const char* type, StringArray tags, std::initializer_list<std::pair<const char*, double>> over)
{
    Preset p; p.name = name; p.type = type; p.tags = tags; p.factory = true;
    for (auto& o : over)
        for (int i = 0; i < kCount; ++i)
            if (String (o.first) == keys[i]) p.v[i] = o.second;
    return p;
}

const std::vector<Preset>& factory()
{
    // Attack and release in ms, converted to knob positions.
    auto ak = [] (double ms) { return 10.0 * std::log (ms / 0.1) / std::log (1000.0); };
    auto rk = [] (double ms) { return 10.0 * std::log (ms / 10.0) / std::log (200.0); };
    static const std::vector<Preset> list {
        mk ("Start Here", "Kick", { "start", "general" }, { { "out", 5 } }),
        mk ("Kick Tight", "Kick", { "tight", "punch" }, { { "thr", 4 }, { "ratio", 5 }, { "out", 5 }, { "hp", 4 } }),
        mk ("Hard Dance Punch", "Kick", { "hard dance", "punch" }, { { "thr", 3.5 }, { "ratio", 7 }, { "out", 6.2 }, { "hp", 5.5 }, { "ceil", 8 } }),
        mk ("Kick Peak Tamer", "Kick", { "limiter", "peaks" }, { { "thr", 6.5 }, { "ratio", 8 }, { "out", 5.8 }, { "hp", 5 }, { "ceil", 8 } }),
        mk ("Kick Soft Squeeze", "Kick", { "gentle", "soft knee" }, { { "thr", 4 }, { "ratio", 3 }, { "knee", 1 }, { "out", 5 }, { "mix", 70 } }),
        mk ("Sub Kick Safe", "Kick", { "sub", "low end" }, { { "thr", 5 }, { "ratio", 5 }, { "out", 5 }, { "hp", 3 } }),
        mk ("Snare Crack", "Snare", { "crack", "bright" }, { { "thr", 4 }, { "ratio", 5 }, { "out", 5.6 }, { "hp", 5 } }),
        mk ("Snare Body", "Snare", { "body", "soft knee" }, { { "thr", 5 }, { "ratio", 4 }, { "knee", 1 }, { "out", 4.8 } }),
        mk ("Clap Pop", "Snare", { "clap", "pop" }, { { "thr", 4.5 }, { "ratio", 6 }, { "out", 5 }, { "hp", 6 } }),
        mk ("Drum Bus Glue", "Drum Bus", { "glue", "soft knee" }, { { "thr", 5.5 }, { "ratio", 2 }, { "knee", 1 }, { "out", 4.3 }, { "hp", 4 } }),
        mk ("Drum Bus Slam", "Drum Bus", { "heavy", "loud" }, { { "thr", 3.5 }, { "ratio", 6 }, { "out", 4.6 }, { "hp", 4 }, { "ceil", 9 } }),
        mk ("Parallel Punch", "Drum Bus", { "parallel", "mix" }, { { "thr", 3 }, { "ratio", 8 }, { "out", 3.7 }, { "hp", 5 }, { "mix", 35 } }),
        mk ("Room Squash", "Drum Bus", { "room", "squash" }, { { "thr", 2.5 }, { "ratio", 7 }, { "out", 4.7 }, { "mix", 60 } }),
        mk ("Hat Control", "Perc", { "hats", "tame" }, { { "thr", 5 }, { "ratio", 4 }, { "out", 4.4 }, { "hp", 8 } }),
        mk ("Mix Bus Gentle", "Bus", { "gentle", "mix bus" }, { { "thr", 6 }, { "ratio", 1 }, { "knee", 1 }, { "out", 4.3 } }),
        mk ("Inverse Duck", "FX", { "negative ratio", "duck" }, { { "thr", 6 }, { "ratio", 9 }, { "out", 3.7 }, { "mix", 50 } }),
        mk ("Kick Punch (Manual)", "Kick", { "manual timing", "punch" }, { { "timing", 1 }, { "atk", ak (30) }, { "rel", rk (100) }, { "thr", 4 }, { "ratio", 5 }, { "out", 5.4 }, { "hp", 4 } }),
        mk ("Snare Snap (Manual)", "Snare", { "manual timing", "snap" }, { { "timing", 1 }, { "atk", ak (12) }, { "rel", rk (60) }, { "thr", 4.5 }, { "ratio", 6 }, { "out", 5.2 }, { "hp", 5 } }),
        mk ("Bass Tighten (Manual)", "Bass", { "manual timing", "tight" }, { { "timing", 1 }, { "atk", ak (5) }, { "rel", rk (250) }, { "thr", 4.5 }, { "ratio", 4 }, { "out", 5.2 }, { "hp", 0 } }),
        mk ("Parallel Drum Crush (Manual)", "Drum Bus", { "manual timing", "parallel" }, { { "timing", 1 }, { "atk", ak (0.5) }, { "rel", rk (60) }, { "thr", 2 }, { "ratio", 8 }, { "out", 4.2 }, { "hp", 4 }, { "mix", 40 } }),
    };
    return list;
}

File userFolder()
{
   #if JUCE_LINUX || JUCE_BSD
    const String xdg = SystemStats::getEnvironmentVariable ("XDG_DATA_HOME", {});
    const File base = (xdg.isNotEmpty() && File::isAbsolutePath (xdg)) ? File (xdg)
                                                                       : File::getSpecialLocation (File::userHomeDirectory).getChildFile (".local/share");
   #else
    const File base = File::getSpecialLocation (File::userApplicationDataDirectory); // %APPDATA% or ~/Library/Application Support
   #endif
    return base.getChildFile ("Studio 87").getChildFile ("PunchPaw").getChildFile ("presets");
}

bool valid (const var& o)
{
    auto* obj = o.getDynamicObject();
    if (obj == nullptr) return false;
    const var name = obj->getProperty ("name"), pv = obj->getProperty ("p");
    if (! name.isString() || name.toString().isEmpty() || name.toString().length() > 60) return false;
    auto* po = pv.getDynamicObject();
    if (po == nullptr) return false;
    for (auto& nv : po->getProperties())
        for (int i = 0; i < kCount; ++i)
            if (nv.name.toString() == keys[i])
            {
                if (! (nv.value.isDouble() || nv.value.isInt() || nv.value.isInt64())) return false;
                const double x = (double) nv.value;
                if (! std::isfinite (x) || x < lo[i] || x > hi[i]) return false;
            }
    return true;
}

Preset fromVar (const var& o)
{
    Preset p;
    p.name = o["name"].toString().substring (0, 60);
    const String t = o["type"].toString();
    p.type = "Bus";
    for (auto* ty : types) if (t == ty) p.type = t;
    if (auto* arr = o["tags"].getArray())
        for (auto& tg : *arr)
            if (tg.isString() && p.tags.size() < 8) p.tags.add (tg.toString().substring (0, 20));
    if (auto* po = o["p"].getDynamicObject())
        for (int i = 0; i < kCount; ++i)
            if (po->hasProperty (keys[i])) p.v[i] = (double) po->getProperty (keys[i]);
    return p;
}

var toVar (const Preset& p)
{
    auto* o = new DynamicObject();
    o->setProperty ("name", p.name);
    o->setProperty ("type", p.type);
    Array<var> tg; for (auto& t : p.tags) tg.add (t);
    o->setProperty ("tags", tg);
    auto* pv = new DynamicObject();
    for (int i = 0; i < kCount; ++i) pv->setProperty (keys[i], p.v[i]);
    o->setProperty ("p", var (pv));
    return var (o);
}

static String fileNameFor (const String& name)
{
    String s = name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-+.");
    s = s.trim().replaceCharacters (" ", "_");
    if (s.isEmpty()) s = "preset";
    return s.substring (0, 60) + "-" + String::toHexString (name.hashCode()) + ".json";
}

Store::Store() { reload(); }

void Store::reload()
{
    mine.clear();
    auto dir = userFolder();
    if (! dir.isDirectory()) return;
    for (auto& f : dir.findChildFiles (File::findFiles, false, "*.json"))
    {
        if (f.getSize() > 20000) continue;
        const var o = JSON::parse (f);
        if (! valid (o)) continue;
        Preset p = fromVar (o);
        bool clash = false;
        for (auto& x : factory()) if (x.name == p.name) clash = true;
        for (auto& x : mine) if (x.name == p.name) clash = true;
        if (! clash) mine.push_back (p);
    }
    std::sort (mine.begin(), mine.end(), [] (const Preset& a, const Preset& b) { return a.name.compareIgnoreCase (b.name) < 0; });
}

std::vector<Preset> Store::all() const
{
    std::vector<Preset> r = factory();
    r.insert (r.end(), mine.begin(), mine.end());
    return r;
}

const Preset* Store::find (const String& name) const
{
    for (auto& p : factory()) if (p.name == name) return &p;
    for (auto& p : mine) if (p.name == name) return &p;
    return nullptr;
}

bool Store::save (const Preset& p, const String& replaceName)
{
    auto dir = userFolder();
    if (! dir.createDirectory().wasOk()) return false;
    const String old = replaceName.isEmpty() ? p.name : replaceName;
    for (auto& f : dir.findChildFiles (File::findFiles, false, "*.json"))
    {
        const var o = JSON::parse (f);
        if (o["name"].toString() == old || o["name"].toString() == p.name) f.deleteFile();
    }
    Preset q = p; q.factory = false;
    if (! dir.getChildFile (fileNameFor (q.name)).replaceWithText (JSON::toString (toVar (q), false))) return false;
    reload();
    return true;
}

bool Store::remove (const String& name)
{
    bool ok = false;
    for (auto& f : userFolder().findChildFiles (File::findFiles, false, "*.json"))
        if (JSON::parse (f)["name"].toString() == name) ok = f.deleteFile() || ok;
    reload();
    return ok;
}

String Store::exportJson (const std::vector<Preset>& list) const
{
    auto* root = new DynamicObject();
    root->setProperty ("punchpaw", 1);
    Array<var> arr; for (auto& p : list) arr.add (toVar (p));
    root->setProperty ("presets", arr);
    return JSON::toString (var (root), false);
}

int Store::importJson (const String& text)
{
    const var o = JSON::parse (text);
    if (! o.isObject() || (int) o["punchpaw"] != 1) return -1;
    auto* arr = o["presets"].getArray();
    if (arr == nullptr || arr->isEmpty() || arr->size() > 200) return -1;
    for (auto& x : *arr) if (! valid (x)) return -1;
    for (auto& x : *arr)
    {
        Preset p = fromVar (x);
        for (auto& f : factory()) if (f.name == p.name) p.name += " (imported)";
        save (p);
    }
    return arr->size();
}
} // namespace presets

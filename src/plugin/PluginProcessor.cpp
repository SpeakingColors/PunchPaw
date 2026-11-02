#include "PluginProcessor.h"
#include "../editor/PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout PunchPawProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const int v = 1; // parameter version

    auto expRange = [] (float lo, float hi)
    {
        const float ratio = hi / lo;
        return NormalisableRange<float> (lo, hi,
            [lo, ratio] (float, float, float n) { return lo * std::pow (ratio, n); },
            [lo, ratio] (float, float, float x) { return std::log (x / lo) / std::log (ratio); },
            [lo, hi] (float, float, float x) { return jlimit (lo, hi, x); });
    };
    auto msText = [] (float ms, int) -> String
    {
        if (ms >= 1000.0f) return String (ms / 1000.0f, 2) + " s";
        if (ms >= 100.0f) return String (roundToInt (ms)) + " ms";
        return String (ms, ms >= 10.0f ? 1 : 2) + " ms";
    };
    using Attr = AudioParameterFloatAttributes;

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::thr, v }, "Threshold",
        NormalisableRange<float> (-40.0f, 0.0f, 0.01f), -20.0f,
        Attr().withLabel ("dBFS").withStringFromValueFunction ([] (float x, int) { return String (x, 1) + " dBFS"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::ratio, v }, "Ratio",
        NormalisableRange<float> (0.0f, 10.0f, 0.001f), 4.0f,
        Attr().withStringFromValueFunction ([] (float x, int) { return String (pp::ratioText (x)); })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::out, v }, "Output",
        NormalisableRange<float> (-20.0f, 20.0f, 0.01f), 0.0f,
        Attr().withLabel ("dB").withStringFromValueFunction ([] (float x, int) { return String (x, 1) + " dB"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::hp, v }, "SC High-pass",
        NormalisableRange<float> (0.0f, 10.0f, 0.001f), 0.0f,
        Attr().withStringFromValueFunction ([] (float x, int) { const float hz = pp::hpHzOf (x); return hz <= 0.0f ? String ("Off") : String (roundToInt (hz)) + " Hz"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, v }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, Attr().withLabel ("%")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::ceil, v }, "Ceiling",
        NormalisableRange<float> (0.0f, 10.0f, 0.001f), 10.0f,
        Attr().withStringFromValueFunction ([] (float x, int) { const float d = pp::ceilDbOf (x); return d >= 50.0f ? String ("Off") : String (d, 1) + " dBFS"; })));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::knee, v }, "Knee", StringArray { "Hard", "Soft" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::timing, v }, "Timing", StringArray { "Auto", "Manual" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::atk, v }, "Attack",
        expRange (0.1f, 100.0f), 10.0f, Attr().withStringFromValueFunction (msText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::rel, v }, "Release",
        expRange (10.0f, 2000.0f), 150.0f, Attr().withStringFromValueFunction (msText)));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, v }, "Bypass", false));
    return { p.begin(), p.end() };
}

PunchPawProcessor::PunchPawProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PUNCHPAW", createLayout())
{
    pThr = apvts.getRawParameterValue (ids::thr);       pRatio = apvts.getRawParameterValue (ids::ratio);
    pOut = apvts.getRawParameterValue (ids::out);       pHp = apvts.getRawParameterValue (ids::hp);
    pMix = apvts.getRawParameterValue (ids::mix);       pCeil = apvts.getRawParameterValue (ids::ceil);
    pKnee = apvts.getRawParameterValue (ids::knee);     pTiming = apvts.getRawParameterValue (ids::timing);
    pAtk = apvts.getRawParameterValue (ids::atk);       pRel = apvts.getRawParameterValue (ids::rel);
    pBypass = apvts.getRawParameterValue (ids::bypass);
}

pp::Params PunchPawProcessor::toCore (float thr, float ratio, float out, float hp, float mix, float ceil, bool soft, bool manual, float atk, float rel)
{
    pp::Params q;
    q.thrDb = thr;
    q.slope = pp::slopeOf (ratio);
    q.outDb = out;
    q.hpHz = pp::hpHzOf (hp);
    q.mix = mix * 0.01f;
    q.ceilDb = pp::ceilDbOf (ceil);
    q.soft = soft;
    q.manual = manual;
    q.atkMs = atk;
    q.relMs = rel;
    return q;
}

pp::Params PunchPawProcessor::readParams() const
{
    return toCore (pThr->load(), pRatio->load(), pOut->load(), pHp->load(), pMix->load(), pCeil->load(),
                   pKnee->load() > 0.5f, pTiming->load() > 0.5f, pAtk->load(), pRel->load());
}

bool PunchPawProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}

var PunchPawProcessor::getSetting (const char* key, const var& fallback) const
{
    return apvts.state.getProperty (key, fallback);
}

void PunchPawProcessor::setSetting (const char* key, const var& v)
{
    apvts.state.setProperty (key, v, nullptr);
}

void PunchPawProcessor::prepareToPlay (double sampleRate, int)
{
    comp.setParams (readParams());
    comp.prepare (sampleRate);
    setLatencySamples (pp::Compressor::kLatency);   // fixed, at every sample rate and setting
}

void PunchPawProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numCh = jmin (2, buffer.getNumChannels());
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    if (numCh == 0) return;

    comp.setParams (readParams());
    comp.process (buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr, buffer.getNumSamples(), pBypass->load() > 0.5f);

    grNow.store (comp.grNow());
    raiseTo (grPeak, comp.grPeakBlock());
    raiseTo (inPeak, comp.inPeakBlock());
    raiseTo (outPeak, comp.outPeakBlock());
    vuIn.store (comp.vuIn());
    vuOut.store (comp.vuOut());
    overDb.store (comp.overDb());
    atkNow.store (comp.autoAttackNow());
}

AudioProcessorEditor* PunchPawProcessor::createEditor()
{
    return new PunchPawEditor (*this);
}

void PunchPawProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PunchPawProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml)); // unknown keys are ignored by APVTS
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PunchPawProcessor(); }

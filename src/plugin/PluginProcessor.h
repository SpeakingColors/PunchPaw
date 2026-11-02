#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../core/PunchPaw.h"
#include <atomic>

// Stable parameter IDs. Never rename these (saved projects use them).
namespace ids
{
    inline constexpr const char* thr = "thr";         // threshold, dBFS
    inline constexpr const char* ratio = "ratio";     // ratio knob position 0..10 (8 is infinity, past it negative)
    inline constexpr const char* out = "out";         // output, dB
    inline constexpr const char* hp = "hp";           // sidechain high-pass knob position 0..10
    inline constexpr const char* mix = "mix";         // percent
    inline constexpr const char* ceil = "ceil";       // ceiling knob position 0..10 (10 is off)
    inline constexpr const char* knee = "knee";       // 0 hard, 1 soft
    inline constexpr const char* timing = "timing";   // 0 auto, 1 manual
    inline constexpr const char* atk = "atk";         // manual attack, ms
    inline constexpr const char* rel = "rel";         // manual release, ms
    inline constexpr const char* bypass = "bypass";
}

// Plugin-wide settings kept in the saved state (not presets, not automatable).
namespace setting
{
    inline constexpr const char* look = "look";       // "A", "B", "C"
    inline constexpr const char* tips = "tips";       // info bubbles
    inline constexpr const char* power = "power";     // power saving redraw
    inline constexpr const char* scale = "scale";     // window scale, 1.0 = 100 %
    inline constexpr const char* preset = "preset";   // name of the loaded preset
    inline constexpr const char* view = "view";       // level meter shows 0 in, 1 out, 2 gain reduction
}

class PunchPawProcessor : public juce::AudioProcessor
{
public:
    PunchPawProcessor();
    ~PunchPawProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PunchPaw"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Meter data for the editor (written on the audio thread, read by the editor timer).
    std::atomic<float> grNow { 0.0f };       // gain reduction, positive dB
    std::atomic<float> grPeak { 0.0f };      // largest gain reduction since the editor last read it (exchange to 0)
    std::atomic<float> inPeak { 0.0f };      // linear peak since the editor last read it (exchange to 0)
    std::atomic<float> outPeak { 0.0f };
    std::atomic<float> vuIn { 0.0f };        // mean square, 65 ms
    std::atomic<float> vuOut { 0.0f };
    std::atomic<float> overDb { -99.0f };    // detector level minus threshold
    std::atomic<float> atkNow { 15.0f };     // auto attack time right now
    const pp::Compressor& core() const { return comp; }

    // Settings stored in the state tree.
    juce::var getSetting (const char* key, const juce::var& fallback) const;
    void setSetting (const char* key, const juce::var& v);
    static void raiseTo (std::atomic<float>& a, float v) { float c = a.load(); while (v > c && ! a.compare_exchange_weak (c, v)) {} }

    static pp::Params toCore (float thr, float ratio, float out, float hp, float mix, float ceil, bool soft, bool manual, float atk, float rel);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    pp::Params readParams() const;

    pp::Compressor comp;
    std::atomic<float>* pThr; std::atomic<float>* pRatio; std::atomic<float>* pOut; std::atomic<float>* pHp;
    std::atomic<float>* pMix; std::atomic<float>* pCeil; std::atomic<float>* pKnee; std::atomic<float>* pTiming;
    std::atomic<float>* pAtk; std::atomic<float>* pRel; std::atomic<float>* pBypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PunchPawProcessor)
};

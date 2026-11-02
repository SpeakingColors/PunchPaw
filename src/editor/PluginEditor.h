#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../plugin/PluginProcessor.h"

class Face;

// Plugin window. The faceplate is a fixed 1200 x 570 drawing that is scaled to the window size;
// the window keeps its proportions and can be dragged from any edge or corner.
class PunchPawEditor : public juce::AudioProcessorEditor
{
public:
    explicit PunchPawEditor (PunchPawProcessor&);
    ~PunchPawEditor() override;
    void resized() override;
    void resetSize();
    void openDialogForTesting (const juce::String& name); // used by tools/snapshot.cpp
    void setViewForTesting (int v);
    void forceRunForTesting();

    static constexpr int kW = 1200, kH = 570;

private:
    PunchPawProcessor& proc;
    std::unique_ptr<Face> face;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::ResizableBorderComponent> border;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PunchPawEditor)
};

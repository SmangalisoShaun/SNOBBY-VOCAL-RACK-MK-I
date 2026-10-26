#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Engines.h"
#include "VocalLearn.h"

class BoomhypeProcessor : public juce::AudioProcessor
{
public:
    BoomhypeProcessor();
    ~BoomhypeProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "BOOMHYPE Vocal Rack"; }
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

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    // rack editing (message thread only)
    int  used() const;
    int  typeAt (int s) const  { return pType[s]->getIndex(); }
    int  styleAt (int s) const { return pStyle[s]->get(); }
    bool bypassAt (int s) const { return pByp[s]->get(); }
    void setModule (int slot, int type, int style);
    bool insertModule (int pos, int type, int style);
    void removeSlot (int pos);
    void moveSlot (int from, int to);

    // presets
    static const juce::StringArray& factoryNames();
    juce::File presetFolder() const;
    juce::StringArray userPresetNames() const;
    void loadFactory (const juce::String& name);
    bool loadUser (const juce::String& name);
    void saveUser (const juce::String& name);
    void deleteUser (const juce::String& name);
    juce::String presetName() const { return apvts.state.getProperty ("presetName", "SNOBBY VOCAL RACK").toString(); }
    bool isDirty() const;
    void markClean (const juce::String& name);
    void switchAB();
    int  abSlot = 0;

    // vocal learning
    enum LearnState { Idle = 0, Capturing, Ready, Analyzing, Done };
    std::atomic<int> learnState { Idle };
    std::atomic<int> learnPos { 0 };
    int learnTotal = 0;
    bh::VocalProfile profile;
    void startLearn();
    void cancelLearn() { learnState = Idle; }
    void startAnalysis();
    void buildChain (const juce::String& style, const bh::VocalProfile& q, bool beatConnected);
    juce::String getProp (const char* k, const char* d) const { return apvts.state.getProperty (k, d).toString(); }
    void setProp (const char* k, const juce::String& v) { apvts.state.setProperty (k, v, nullptr); }

    // meters
    std::atomic<float> slotGr[bh::kSlots], slotLvl[bh::kSlots], inPeakDb { -100.f }, outPeakDb { -100.f }, scLevelDb { -100.f };

private:
    struct Analysis : juce::Thread
    {
        explicit Analysis (BoomhypeProcessor& o) : juce::Thread ("vocal analysis"), owner (o) {}
        void run() override;
        BoomhypeProcessor& owner;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::String signature() const;
    void setNorm (juce::RangedAudioParameter* p, float n) const { p->setValueNotifyingHost (n); }

    struct SlotData { float type, style, byp, k[bh::kKnobs], w[bh::kSw]; };
    SlotData readSlot (int s) const;
    void writeSlot (int s, const SlotData& d);
    void setK (int slot, int j, float realValue);
    void setW (int slot, int j, int v);

    juce::AudioParameterChoice* pType[bh::kSlots];
    juce::AudioParameterInt*    pStyle[bh::kSlots];
    juce::AudioParameterBool*   pByp[bh::kSlots];
    juce::AudioParameterFloat*  pK[bh::kSlots][bh::kKnobs];
    juce::AudioParameterInt*    pW[bh::kSlots][bh::kSw];
    juce::AudioParameterFloat  *pIn, *pOut;
    juce::AudioParameterBool*   pLevelMatch;

    std::unique_ptr<bh::Engine> engines[bh::kSlots][bh::NumTypes];
    int lastType[bh::kSlots] {};
    juce::String abXml[2];

    std::vector<float> learnVox, learnBeat;
    std::unique_ptr<Analysis> analysis;
    juce::AudioBuffer<float> work;
    float lmIn = 1.0e-6f, lmOut = 1.0e-6f, lmGain = 1.f;
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoomhypeProcessor)
};

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace bh;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BoomhypeProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    const StringArray typeNames { "Empty", "Parametric EQ", "De-Esser", "76 FET", "2A Opto", "1B Opto", "670 Tube", "SNOBTECH EQ", "Air EQ" };

    for (int s = 0; s < kSlots; ++s)
    {
        const String n = "Unit " + String (s + 1) + " ";
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pidType (s), 1 }, n + "Module", typeNames, 0));
        l.add (std::make_unique<AudioParameterInt> (ParameterID { pidStyle (s), 1 }, n + "Style", 0, 4, 0));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { pidByp (s), 1 }, n + "Bypass", false));
        for (int j = 0; j < kKnobs; ++j)
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { pidKnob (s, j), 1 }, n + "Knob " + String (j + 1), NormalisableRange<float> (0.f, 1.f), 0.f));
        for (int j = 0; j < kSw; ++j)
            l.add (std::make_unique<AudioParameterInt> (ParameterID { pidSw (s, j), 1 }, n + "Switch " + String (j + 1), 0, 255, 0));
    }
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "in_trim", 1 },  "Input Trim",  NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "out_trim", 1 }, "Output Trim", NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "level_match", 1 }, "Level Match", false));
    return l;
}

BoomhypeProcessor::BoomhypeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain (beat)", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "BOOMHYPE", createLayout())
{
    defs();   // build the module tables on this thread
    for (int s = 0; s < kSlots; ++s)
    {
        pType[s]  = static_cast<juce::AudioParameterChoice*> (apvts.getParameter (pidType (s)));
        pStyle[s] = static_cast<juce::AudioParameterInt*> (apvts.getParameter (pidStyle (s)));
        pByp[s]   = static_cast<juce::AudioParameterBool*> (apvts.getParameter (pidByp (s)));
        for (int j = 0; j < kKnobs; ++j) pK[s][j] = static_cast<juce::AudioParameterFloat*> (apvts.getParameter (pidKnob (s, j)));
        for (int j = 0; j < kSw; ++j)    pW[s][j] = static_cast<juce::AudioParameterInt*> (apvts.getParameter (pidSw (s, j)));
        for (int t = 1; t < NumTypes; ++t) engines[s][t] = makeEngine (t);
        slotGr[s] = 0.f; slotLvl[s] = -100.f;
    }
    pIn = static_cast<juce::AudioParameterFloat*> (apvts.getParameter ("in_trim"));
    pOut = static_cast<juce::AudioParameterFloat*> (apvts.getParameter ("out_trim"));
    pLevelMatch = static_cast<juce::AudioParameterBool*> (apvts.getParameter ("level_match"));

    loadFactory ("SNOBBY VOCAL RACK");     // what you get on a fresh instance
}

BoomhypeProcessor::~BoomhypeProcessor()
{
    if (analysis) analysis->stopThread (2000);
}

bool BoomhypeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto m = l.getMainInputChannelSet();
    if (m != l.getMainOutputChannelSet()) return false;
    if (m != juce::AudioChannelSet::mono() && m != juce::AudioChannelSet::stereo()) return false;
    if (l.inputBuses.size() > 1) { const auto sc = l.inputBuses[1]; if (! sc.isDisabled() && sc.size() > 2) return false; }
    return true;
}

void BoomhypeProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (int s = 0; s < kSlots; ++s)
        for (int t = 1; t < NumTypes; ++t) if (engines[s][t]) engines[s][t]->prepare (sampleRate);
    learnTotal = (int) (sampleRate * 12.0);
    if ((int) learnVox.size() != learnTotal) { learnVox.assign ((size_t) learnTotal, 0.f); learnBeat.assign ((size_t) learnTotal, 0.f); }
    lmIn = lmOut = 1.0e-6f; lmGain = 1.f;
}

//==============================================================================
void BoomhypeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto mainBus = getBusBuffer (buffer, true, 0);
    const int n = mainBus.getNumSamples(), nch = std::min (mainBus.getNumChannels(), 2);
    if (nch == 0 || n == 0) return;

    // ---- beat on the sidechain input
    const bool hasSc = getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled();
    const float* scCh[2] = { nullptr, nullptr };
    int scN = 0;
    if (hasSc)
    {
        auto sc = getBusBuffer (buffer, true, 1);
        scN = std::min (sc.getNumChannels(), 2);
        float m = 0;
        for (int c = 0; c < scN; ++c) { scCh[c] = sc.getReadPointer (c); m = std::max (m, sc.getMagnitude (c, 0, n)); }
        scLevelDb = std::max (linToDb (m), scLevelDb.load() - 0.5f);
    }
    else scLevelDb = -100.f;

    // ---- LEARN VOCAL capture (pre-trim, straight from the input)
    if (learnState.load() == Capturing)
    {
        int pos = learnPos.load();
        for (int i = 0; i < n && pos < learnTotal; ++i, ++pos)
        {
            float v = 0; for (int c = 0; c < nch; ++c) v += mainBus.getSample (c, i);
            learnVox[(size_t) pos] = v / (float) nch;
            if (scN > 0) { float b = 0; for (int c = 0; c < scN; ++c) b += scCh[c][i]; learnBeat[(size_t) pos] = b / (float) scN; }
        }
        learnPos = pos;
        if (pos >= learnTotal) learnState = Ready;
    }

    // ---- input trim
    mainBus.applyGain (dbToLin (pIn->get()));
    float* ch[2] = { mainBus.getWritePointer (0), nch > 1 ? mainBus.getWritePointer (1) : nullptr };
    inPeakDb = linToDb (std::max (mainBus.getMagnitude (0, 0, n), nch > 1 ? mainBus.getMagnitude (1, 0, n) : 0.f));

    double inMs = 0;
    for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) inMs += (double) ch[c][i] * ch[c][i];
    inMs /= (double) (n * nch);

    // ---- the rack
    for (int s = 0; s < kSlots; ++s)
    {
        const int type = pType[s]->getIndex();
        if (type != lastType[s]) { if (engines[s][type]) engines[s][type]->reset(); lastType[s] = type; }
        if (type == Empty || engines[s][type] == nullptr) { slotGr[s] = 0.f; slotLvl[s] = -100.f; continue; }
        if (pByp[s]->get()) { slotGr[s] = 0.f; continue; }

        const int style = pStyle[s]->get();
        const auto& d = getDef (type, style);
        SlotVals v; v.style = style;
        for (int j = 0; j < d.nKnobs(); ++j) v.k[j] = d.real (j, pK[s][j]->get());
        for (int j = 0; j < kSw; ++j) v.w[j] = pW[s][j]->get();

        auto& e = *engines[s][type];
        e.process (ch, nch, n, v);
        slotGr[s] = e.gr.load();

        double ms = 0; for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) ms += (double) ch[c][i] * ch[c][i];
        slotLvl[s] = 10.f * std::log10 ((float) (ms / (double) (n * nch)) + 1.0e-12f);
    }

    // ---- level match (A/B at equal loudness) and output trim
    double outMs = 0;
    for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) outMs += (double) ch[c][i] * ch[c][i];
    outMs /= (double) (n * nch);
    const float k = 1.f - std::exp (-(float) n / (float) (sr * 1.5));
    lmIn += k * ((float) inMs - lmIn); lmOut += k * ((float) outMs - lmOut);
    float target = 1.f;
    if (pLevelMatch->get() && lmIn > 1.0e-9f && lmOut > 1.0e-9f) target = juce::jlimit (0.25f, 4.f, std::sqrt (lmIn / lmOut));
    const float g0 = lmGain, g1 = target * dbToLin (pOut->get());
    for (int c = 0; c < nch; ++c) mainBus.applyGainRamp (c, 0, n, g0 * dbToLin (0.f), g1);
    lmGain = g1;
    outPeakDb = linToDb (std::max (mainBus.getMagnitude (0, 0, n), nch > 1 ? mainBus.getMagnitude (1, 0, n) : 0.f));
}

juce::AudioProcessorEditor* BoomhypeProcessor::createEditor() { return new BoomhypeEditor (*this); }

//==============================================================================
// rack editing
int BoomhypeProcessor::used() const
{
    int u = 0;
    for (int s = 0; s < kSlots; ++s) if (typeAt (s) != Empty) u = s + 1;
    return u;
}

BoomhypeProcessor::SlotData BoomhypeProcessor::readSlot (int s) const
{
    SlotData d;
    auto nv = [] (juce::RangedAudioParameter* q) { return q->getValue(); };
    d.type = nv (pType[s]); d.style = nv (pStyle[s]); d.byp = nv (pByp[s]);
    for (int j = 0; j < kKnobs; ++j) d.k[j] = nv (pK[s][j]);
    for (int j = 0; j < kSw; ++j) d.w[j] = nv (pW[s][j]);
    return d;
}

void BoomhypeProcessor::writeSlot (int s, const SlotData& d)
{
    setNorm (pType[s], d.type); setNorm (pStyle[s], d.style); setNorm (pByp[s], d.byp);
    for (int j = 0; j < kKnobs; ++j) setNorm (pK[s][j], d.k[j]);
    for (int j = 0; j < kSw; ++j) setNorm (pW[s][j], d.w[j]);
}

void BoomhypeProcessor::setModule (int slot, int type, int style)
{
    const auto& d = getDef (type, style);
    setNorm (pType[slot], pType[slot]->convertTo0to1 ((float) type));
    setNorm (pStyle[slot], pStyle[slot]->convertTo0to1 ((float) style));
    setNorm (pByp[slot], 0.f);
    for (int j = 0; j < kKnobs; ++j) setNorm (pK[slot][j], j < d.nKnobs() ? d.defNorm (j) : 0.f);
    for (int j = 0; j < kSw; ++j) setNorm (pW[slot][j], pW[slot][j]->convertTo0to1 ((float) (j < (int) d.sw.size() ? d.sw[(size_t) j].def : 0)));
}

bool BoomhypeProcessor::insertModule (int pos, int type, int style)
{
    const int u = used();
    if (u >= kSlots) return false;
    pos = juce::jlimit (0, u, pos);
    for (int s = u; s > pos; --s) writeSlot (s, readSlot (s - 1));
    setModule (pos, type, style);
    return true;
}

void BoomhypeProcessor::removeSlot (int pos)
{
    const int u = used();
    if (pos < 0 || pos >= u) return;
    for (int s = pos; s < u - 1; ++s) writeSlot (s, readSlot (s + 1));
    setModule (u - 1, Empty, 0);
}

void BoomhypeProcessor::moveSlot (int from, int to)
{
    const int u = used();
    if (from < 0 || from >= u) return;
    to = juce::jlimit (0, u - 1, to);
    if (from == to) return;
    const auto tmp = readSlot (from);
    if (from < to) for (int s = from; s < to; ++s) writeSlot (s, readSlot (s + 1));
    else           for (int s = from; s > to; --s) writeSlot (s, readSlot (s - 1));
    writeSlot (to, tmp);
}

void BoomhypeProcessor::setK (int slot, int j, float realValue)
{
    const auto& d = getDef (typeAt (slot), styleAt (slot));
    if (j < d.nKnobs()) setNorm (pK[slot][j], d.norm (j, realValue));
}
void BoomhypeProcessor::setW (int slot, int j, int v) { setNorm (pW[slot][j], pW[slot][j]->convertTo0to1 ((float) v)); }

//==============================================================================
// the vocal chain builder: starting points per style (from the chain research), adjusted by what LEARN VOCAL heard
void BoomhypeProcessor::buildChain (const juce::String& style, const VocalProfile& q, bool beatConnected)
{
    struct PF { const char* name; int c1; float deA, deB, pres, inp; float att, rel; int ratio; float gr1, peak, gain; int limit; float gr2; int sn, tc; float i670, gr3; };
    static const PF pf[] = {
        { "RAP",    1, -4, -3, 3.0f, 8, 3, 6, 3, 5.5f, 30, 2, 0, 1.5f, 0, 3, 3, 1.5f },
        { "R&B",    4, -2, -2, 1.0f, 6, 2, 4, 3, 3.5f, 40, 3, 0, 3.0f, 1, 5, 2, 1.0f },
        { "POP",    4, -3, -3, 2.0f, 7, 3, 5, 3, 5.0f, 32, 2, 0, 2.0f, 1, 4, 3, 1.5f },
        { "ROCK",   1, -3, -3, 2.5f, 9, 4, 6, 2, 6.0f, 35, 2, 1, 2.0f, 0, 3, 4, 2.0f },
        { "BALLAD", 4, -2, -2, 1.0f, 5, 2, 3, 3, 3.0f, 38, 3, 0, 3.0f, 1, 6, 2, 1.0f } };
    const PF* f = &pf[0];
    for (auto& p : pf) if (style == p.name) f = &p;

    const bool male = q.male;
    for (int s = 0; s < kSlots; ++s) setModule (s, Empty, 0);

    // 1  surgical EQ: high-pass, cut at the resonance, presence
    setModule (0, ParamEQ, 0);
    setK (0, 0, male ? 65.f : 90.f);
    setK (0, 1, (beatConnected && q.beatMuddy) ? -5.f : -4.f);
    setK (0, 2, (q.valid && ! q.spikeFound) ? 300.f : q.spike);
    setK (0, 3, f->pres);
    setK (0, 4, male ? 3500.f : 4500.f);
    setW (0, 0, 7);

    // 2  de-esser A, aimed at the sibilance that was heard
    setModule (1, DeEsser, 0);
    setK (1, 0, q.sibPk * 1000.f); setK (1, 1, f->deA); setK (1, 2, 8.f);

    // 3  FET compressor: input is chosen so the peaks land on the target gain reduction
    setModule (2, FET76, f->c1);
    static const float ratios[5] = { 20.f, 12.f, 8.f, 4.f, 20.f };
    const float R = ratios[f->ratio];
    float input = f->inp;
    if (q.valid) input = juce::jlimit (-6.f, 20.f, -10.f - q.peakDb + f->gr1 / (1.f - 1.f / R));
    setK (2, 0, input); setK (2, 1, 100.f); setK (2, 2, juce::jlimit (-20.f, 20.f, f->gr1 - input + 0.5f));
    setK (2, 3, f->att); setK (2, 4, f->rel);
    setW (2, 0, f->ratio); setW (2, 1, 0);

    // 4  2A opto leveler
    setModule (3, Opto2A, 0);
    setK (3, 0, f->peak + (q.valid ? juce::jlimit (-8.f, 8.f, (q.dyn - 14.f) * 0.8f) : 0.f));
    setK (3, 1, f->gain); setK (3, 2, 0.f); setW (3, 0, f->limit);

    // 5  SNOBTECH EQ: mid-range for rap/rock, low/high for the smoother styles
    setModule (4, SnobTech, f->sn);
    if (f->sn == 0) { setK (4, 0, 0); setK (4, 1, 0); setK (4, 2, 1); setK (4, 3, 3.f); setK (4, 4, male ? 2.f : 3.f); setK (4, 5, 3.5f); }
    else { setK (4, 0, 2); setK (4, 1, 3.f); setK (4, 2, 2.f); setK (4, 3, q.dull ? 5.f : 4.f); setK (4, 4, 6.f); setK (4, 5, q.dull ? 4.5f : 3.5f); setK (4, 6, 1); setK (4, 7, 1.f); }
    setW (4, 0, 1);

    // 6  de-esser B, last cleanup
    setModule (5, DeEsser, 0);
    setK (5, 0, q.sibPk * 1000.f); setK (5, 1, f->deB); setK (5, 2, 6.f);

    // 7  670 glue
    setModule (6, Tube670, 0);
    setK (6, 0, f->i670); setK (6, 1, 5.f); setK (6, 2, 1); setK (6, 3, (float) (f->tc - 1)); setK (6, 4, 0); setK (6, 5, 0.f);
}

//==============================================================================
// presets
const juce::StringArray& BoomhypeProcessor::factoryNames()
{
    static const juce::StringArray n { "SNOBBY VOCAL RACK", "R&B VOCAL", "POP VOCAL", "ROCK VOCAL", "BALLAD VOCAL" };
    return n;
}

static juce::String styleForFactory (const juce::String& n)
{
    if (n == "R&B VOCAL") return "R&B";
    if (n == "POP VOCAL") return "POP";
    if (n == "ROCK VOCAL") return "ROCK";
    if (n == "BALLAD VOCAL") return "BALLAD";
    return "RAP";
}

void BoomhypeProcessor::loadFactory (const juce::String& name)
{
    const auto st = styleForFactory (name);
    buildChain (st, VocalProfile {}, false);
    setProp ("style", st);
    markClean (name);
}

juce::File BoomhypeProcessor::presetFolder() const
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("BOOMHYPE").getChildFile ("Vocal Chains");
    f.createDirectory();
    return f;
}

juce::StringArray BoomhypeProcessor::userPresetNames() const
{
    juce::StringArray out;
    for (auto& f : presetFolder().findChildFiles (juce::File::findFiles, false, "*.bhchain")) out.add (f.getFileNameWithoutExtension());
    out.sort (true);
    return out;
}

void BoomhypeProcessor::saveUser (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return;
    markClean (clean);
    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (presetFolder().getChildFile (clean + ".bhchain"));
}

bool BoomhypeProcessor::loadUser (const juce::String& name)
{
    auto file = presetFolder().getChildFile (name + ".bhchain");
    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            markClean (name);
            return true;
        }
    return false;
}

void BoomhypeProcessor::deleteUser (const juce::String& name) { presetFolder().getChildFile (name + ".bhchain").deleteFile(); }

juce::String BoomhypeProcessor::signature() const
{
    juce::String s;
    for (int i = 0; i < kSlots; ++i)
    {
        s << pType[i]->getIndex() << ',' << pStyle[i]->get() << ',' << (int) pByp[i]->get() << ',';
        for (int j = 0; j < kKnobs; ++j) s << juce::String (pK[i][j]->get(), 3) << ',';
        for (int j = 0; j < kSw; ++j) s << pW[i][j]->get() << ',';
    }
    s << juce::String (pIn->get(), 1) << ',' << juce::String (pOut->get(), 1);
    return s;
}

void BoomhypeProcessor::markClean (const juce::String& name)
{
    setProp ("presetName", name);
    setProp ("presetSig", signature());
}

bool BoomhypeProcessor::isDirty() const { return signature() != apvts.state.getProperty ("presetSig").toString(); }

void BoomhypeProcessor::switchAB()
{
    abXml[abSlot] = apvts.copyState().toXmlString();
    abSlot = 1 - abSlot;
    if (abXml[abSlot].isNotEmpty())
        if (auto xml = juce::XmlDocument::parse (abXml[abSlot]))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// LEARN VOCAL
void BoomhypeProcessor::startLearn()
{
    if (learnState.load() == Analyzing) return;
    learnState = Idle;
    learnPos = 0;
    learnState = Capturing;
}

void BoomhypeProcessor::startAnalysis()
{
    if (learnState.load() != Ready) return;
    learnState = Analyzing;
    if (analysis) analysis->stopThread (2000);
    analysis = std::make_unique<Analysis> (*this);
    analysis->startThread();
}

void BoomhypeProcessor::Analysis::run()
{
    const int n = std::min (owner.learnPos.load(), owner.learnTotal);
    std::vector<float> v (owner.learnVox.begin(), owner.learnVox.begin() + n), b (owner.learnBeat.begin(), owner.learnBeat.begin() + n);
    owner.profile = VocalAnalyzer::analyze (v, b, owner.sr);
    owner.learnState = Done;
}

//==============================================================================
void BoomhypeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void BoomhypeProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));   // chain, knobs AND the preset name come back with the project
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BoomhypeProcessor(); }

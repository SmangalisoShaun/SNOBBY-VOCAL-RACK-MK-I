#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

namespace ui
{
using namespace juce;

inline Font F (float h, bool bold = false) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }
inline Font FM (float h) { return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, Font::plain)); }
inline float rad (float deg) { return deg * MathConstants<float>::pi / 180.f; }
inline Point<float> onCircle (Point<float> c, float r, float fromTop) { const float a = rad (fromTop); return { c.x + std::sin (a) * r, c.y - std::cos (a) * r }; }

static const Colour amber (0xfff0b050), amberDim (0xffe8a040), screen (0xff0a0a08);

//==============================================================================
struct Knob : Slider
{
    int steps = 0;
    double snapValue (double v, DragMode) override { return steps > 1 ? std::round (v * (steps - 1)) / (steps - 1) : v; }
};

struct RackLNF : LookAndFeel_V4
{
    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider& s) override
    {
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (1.f);
        const float d = jmin (b.getWidth(), b.getHeight()); const auto c = b.getCentre();
        const bool dark = (bool) s.getProperties().getWithDefault ("dark", true);
        const auto a = rad (225.f + 270.f * pos);
        Rectangle<float> r (c.x - d / 2, c.y - d / 2, d, d);
        g.setColour (Colours::black.withAlpha (0.55f)); g.fillEllipse (r.translated (0, 3).expanded (1));
        g.setGradientFill (ColourGradient (dark ? Colour (0xff4a4a4a) : Colour (0xfff0f0f0), c.x - d * 0.2f, c.y - d * 0.3f,
                                           dark ? Colour (0xff0e0e0e) : Colour (0xff7a7a7a), c.x + d * 0.4f, c.y + d * 0.5f, true));
        g.fillEllipse (r);
        g.setColour (Colours::black); g.drawEllipse (r, 1.f);
        if (dark)   // fluted edge
        {
            g.setColour (Colour (0x30ffffff));
            for (int i = 0; i < 28; ++i) { auto p1 = onCircle (c, d * 0.40f, i * 360.f / 28), p2 = onCircle (c, d * 0.5f, i * 360.f / 28); g.drawLine ({ p1, p2 }, 0.8f); }
        }
        auto cap = r.reduced (d * 0.14f);
        g.setGradientFill (ColourGradient (dark ? Colour (0xff3c3c3c) : Colour (0xfff7f7f7), c.x - d * 0.15f, c.y - d * 0.2f,
                                           dark ? Colour (0xff080808) : Colour (0xffb8b8b8), c.x + d * 0.3f, c.y + d * 0.35f, true));
        g.fillEllipse (cap);
        g.setColour (dark ? Colour (0xffeeeeee) : Colour (0xff111111));
        g.drawLine ({ onCircle (c, d * 0.12f, a * 180.f / MathConstants<float>::pi), onCircle (c, d * 0.44f, a * 180.f / MathConstants<float>::pi) }, 2.2f);
    }
};

//==============================================================================
// VU drawn on a 128 x 112 design grid
inline void drawVU (Graphics& g, Rectangle<float> r, bool grMode, float value, bool amberFace, bool off)
{
    g.saveState();
    g.addTransform (AffineTransform::scale (r.getWidth() / 128.f).translated (r.getX(), r.getY()));
    auto P = [] (float f, float rr) { return onCircle ({ 64.f, 96.f }, rr, -50.f + 100.f * f); };
    g.setColour (Colour (0xff1b1b1b)); g.fillRoundedRectangle (0, 0, 128, 112, 6);
    const Colour face = amberFace ? Colour (0xfff2b45a) : Colour (0xffefe4a0);
    g.setGradientFill (ColourGradient (face.brighter (0.25f), 64, 90, face.darker (0.35f), 64, 0, true));
    g.fillRoundedRectangle (9, 9, 110, 74, 9);
    g.setColour (Colour (0xff7a4a10)); g.setFont (F (9.f, true));
    g.drawText (grMode ? "GR" : "VU", 14, 12, 30, 12, Justification::left);
    g.setColour (grMode ? Colour (0xff7a4a10) : Colour (0xffc8281c)); g.drawText (grMode ? "dB" : "VU", 76, 12, 36, 12, Justification::centred);

    auto line = [&] (float f, float r1, float r2, Colour c, float w) { g.setColour (c); g.drawLine ({ P (f, r1), P (f, r2) }, w); };
    auto lab = [&] (float f, float rr, const String& t, Colour c, float fs) { auto p = P (f, rr); g.setColour (c); g.setFont (F (fs, true)); g.drawText (t, Rectangle<float> (p.x - 12, p.y - 6, 24, 12), Justification::centred); };
    const Colour red (0xffc8281c), blk (0xff222222);
    auto arc = [&] (float f1, float f2, float rr, Colour c, float w)
    { Path p; p.addCentredArc (64.f, 96.f, rr, rr, 0.f, rad (-50.f + 100.f * f1), rad (-50.f + 100.f * f2), true); g.setColour (c); g.strokePath (p, PathStrokeType (w)); };

    if (grMode)
    {
        auto fg = [] (float gdb) { return 1.f - std::sqrt (gdb / 20.f); };
        for (int i = 0; i <= 20; ++i) if (i <= 12 || i % 2 == 0) line (fg ((float) i), 63, 66, i >= 12 ? red : blk, 0.8f);
        arc (0.f, fg (12.f), 64.5f, red, 3.2f); arc (fg (12.f), 1.f, 64.5f, blk, 1.f);
        for (int gg : { 20, 12, 8, 6, 4, 2, 0 }) { const auto c = gg >= 12 ? red : Colour (0xff1a1a1a); line (fg ((float) gg), 59, 67, c, 1.6f); lab (fg ((float) gg), 51, String (gg), c, 8.5f); }
        g.setColour (Colour (0xff6a4a1a)); g.setFont (F (5.5f)); g.drawText ("GAIN REDUCTION - dB", 14, 69, 100, 8, Justification::centred);
    }
    else
    {
        for (int i = 0; i <= 20; ++i) line (i / 20.f, 63, 66, i >= 14 ? red : blk, 0.8f);
        arc (0.f, 0.7f, 64.5f, blk, 1.f); arc (0.7f, 1.f, 64.5f, red, 3.2f);
        const char* t[] = { "-20", "-10", "-7", "-5", "-3", "-2", "-1", "0", "+1", "+2", "+3" };
        for (int i = 0; i <= 10; ++i) { const auto c = i >= 7 ? red : Colour (0xff1a1a1a); line (i / 10.f, 59, 67, c, 1.6f); lab (i / 10.f, 52, t[i], c, 7.f); }
        g.setColour (Colour (0xff6a4a1a)); g.setFont (F (5.5f)); g.drawText ("OUTPUT LEVEL", 14, 69, 100, 8, Justification::centred);
    }

    float f;
    if (off) f = grMode ? 1.f : 0.f;
    else if (grMode) f = 1.f - std::sqrt (jlimit (0.f, 20.f, value) / 20.f);
    else
    {
        static const float vs[] = { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
        const float v = jlimit (-20.f, 3.f, value); int i = 0; while (i < 9 && v > vs[i + 1]) ++i;
        f = (i + (v - vs[i]) / (vs[i + 1] - vs[i])) / 10.f;
    }
    g.saveState(); g.reduceClipRegion (Rectangle<int> (9, 9, 110, 74));
    g.setColour (Colour (0xff111111)); g.drawLine ({ P (f, 0.f), P (f, 62.f) }, 1.5f);
    g.restoreState();
    g.setColour (Colour (0xff1b1b1b)); g.fillRect (0.f, 83.f, 128.f, 29.f); g.fillEllipse (43.f, 75.f, 42.f, 18.f);
    g.setColour (Colour (0xff3a3a3a)); for (float x : { 30.f, 35.f, 40.f, 88.f, 93.f, 98.f }) g.drawLine (x, 90, x, 106, 1.4f);
    g.setColour (Colour (0xff6c6c6c)); g.fillEllipse (58.5f, 90.5f, 11.f, 11.f);
    if (grMode && ! off)       // the GR number, on the meter
    {
        g.setColour (Colour (0xfff2b45a)); g.setFont (FM (11.f)); g.drawText ("-" + String (jlimit (0.f, 99.f, value), 1), 40, 94, 48, 14, Justification::centred);
    }
    g.restoreState();
}

//==============================================================================
struct Item { int type, style; String label; };
inline Array<Item> itemsFor (int cat)
{
    Array<Item> a;
    using namespace bh;
    if (cat == 0)
    {
        for (int i = 0; i < 5; ++i) a.add ({ FET76, i, "76 - " + getDef (FET76).styles[(size_t) i].name });
        for (int i = 0; i < 3; ++i) a.add ({ Opto2A, i, "2A - " + getDef (Opto2A).styles[(size_t) i].name });
        a.add ({ Opto1B, 0, "1B OPTO - CL1B-STYLE" }); a.add ({ Tube670, 0, "670 TUBE LIMITER" });
    }
    else if (cat == 1) { a.add ({ ParamEQ, 0, "PARAMETRIC EQ" }); a.add ({ SnobTech, 0, "SNOBTECH - MID-RANGE" }); a.add ({ SnobTech, 1, "SNOBTECH - LOW / HIGH" }); a.add ({ AirEQ, 0, "AIR EQ - MAAG-STYLE" }); }
    else a.add ({ DeEsser, 0, "DE-ESSER" });
    return a;
}

struct Grip : Component
{
    int idx = 0;
    void paint (Graphics& g) override { g.setColour (Colour (0xff222222)); g.setFont (F (14.f, true)); g.drawText (String::fromUTF8 ("\xe2\xa0\xbf"), getLocalBounds(), Justification::centred); }
    void mouseDrag (const MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() > 5)
            if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
                if (! c->isDragAndDropActive()) c->startDragging ("slot:" + String (idx), getParentComponent());
    }
    MouseCursor getMouseCursor() override { return MouseCursor::DraggingHandCursor; }
};

class BoomhypeEditor;

//==============================================================================
class SlotComponent : public Component, public DragAndDropTarget
{
public:
    SlotComponent (BoomhypeEditor& e, BoomhypeProcessor& p, int slotIndex);
    int heightNeeded() const { return type == bh::Tube670 ? 176 : 156; }

    void paint (Graphics& g) override;
    void paintOverChildren (Graphics& g) override;
    void resized() override;
    void mouseDown (const MouseEvent& e) override;
    void tick();

    bool isInterestedInDragSource (const SourceDetails& d) override { const auto s = d.description.toString(); return s.startsWith ("mod:") || s.startsWith ("slot:"); }
    void itemDragEnter (const SourceDetails& d) override { itemDragMove (d); }
    void itemDragMove (const SourceDetails& d) override;
    void itemDragExit (const SourceDetails&) override { dragMode = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override;

private:
    BoomhypeEditor& ed; BoomhypeProcessor& proc; const int idx;
    int type, style; const bh::ModDef* def;
    OwnedArray<Knob> knobs;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> att;
    OwnedArray<TextButton> swBtns; struct SwRef { int group, j; bool radio; }; std::vector<SwRef> swRefs;
    TextButton upB { String::fromUTF8 ("\xe2\x96\xb2") }, dnB { String::fromUTF8 ("\xe2\x96\xbc") }, delB { String::fromUTF8 ("\xe2\x9c\x95  REMOVE") }, bypB { "BYPASS" }, swapB { String::fromUTF8 ("\xe2\x87\x84  SWAP COMPRESSOR") };
    ComboBox styleBox; Grip grip;
    Rectangle<int> faceR, vuR, ledR; Array<Rectangle<int>> swTitleR; StringArray swTitles;
    float grDisp = 0, lvlDisp = -30.f; int dragMode = -1;

    bool isComp() const { return type >= bh::FET76 && type <= bh::Tube670; }
    int getSw (int j) const { return static_cast<AudioParameterInt*> (proc.apvts.getParameter (bh::pidSw (idx, j)))->get(); }
    void setSw (int j, int v) { auto* p = proc.apvts.getParameter (bh::pidSw (idx, j)); p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 ((float) v)); p->endChangeGesture(); }
    int meterMode() const;      // 0 = GR, 1 = level, 2 = off
    bool isDeB() const;
    Colour faceTop() const; Colour faceBottom() const; Colour textCol() const;
};

//==============================================================================
struct TrayCard : Component
{
    TrayCard (BoomhypeEditor& e, Item i) : ed (e), item (i) { setMouseCursor (MouseCursor::DraggingHandCursor); }
    void paint (Graphics& g) override
    {
        const auto& st = bh::getStyle (item.type, item.style); const auto& d = bh::getDef (item.type, item.style);
        auto b = getLocalBounds().toFloat();
        g.setColour (Colours::black); g.fillRoundedRectangle (b, 3.f);
        auto f = b.reduced (1.f);
        g.setGradientFill (ColourGradient (Colour (st.face), 0, 0, Colour (st.face2), 0, f.getHeight(), false)); g.fillRoundedRectangle (f, 2.f);
        g.setColour (Colour (0xff8a8d91)); g.fillRect (f.removeFromLeft (8.f)); g.fillRect (f.removeFromRight (8.f));
        g.setColour (d.nameCol ? Colour (d.nameCol) : Colour (st.text)); g.setFont (F (11.f, true));
        g.drawText (item.label, getLocalBounds().reduced (16, 4).removeFromTop (22), Justification::centredLeft);
        for (int j = 0; j < jmin (5, d.nKnobs()); ++j)
        { auto c = Point<float> (24.f + j * 26.f, 44.f); g.setColour (Colours::black); g.fillEllipse (c.x - 9, c.y - 9, 18, 18); g.setColour (Colour (0xff777777)); g.fillEllipse (c.x - 5, c.y - 5, 10, 10); g.setColour (Colours::white); g.drawLine (c.x, c.y - 3, c.x, c.y - 8, 1.5f); }
    }
    void mouseDrag (const MouseEvent& e) override;
    void mouseUp (const MouseEvent& e) override;
    BoomhypeEditor& ed; Item item;
};

//==============================================================================
class BoomhypeEditor : public AudioProcessorEditor, public DragAndDropContainer, private Timer,
                       private AsyncUpdater, private AudioProcessorValueTreeState::Listener
{
public:
    explicit BoomhypeEditor (BoomhypeProcessor& p);
    ~BoomhypeEditor() override;
    void resized() override;
    void paint (Graphics&) override { }

    void addModule (int type, int style) { if (! proc.insertModule (proc.used(), type, style)) flash ("RACK FULL (10 UNITS)"); rebuild(); }
    void handleDrop (const String& desc, int target, int mode);
    void swapMenu (int slot);
    void rebuild() { triggerAsyncUpdate(); }
    void flash (const String& s) { status = s; statusUntil = Time::getMillisecondCounter() + 2500; }
    BoomhypeProcessor& proc;

private:
    friend class SlotComponent;
    struct Content : Component
    {
        BoomhypeEditor& o; explicit Content (BoomhypeEditor& e) : o (e) {}
        void paint (Graphics& g) override;
    };
    Content content { *this };
    RackLNF lnf;
    Viewport vp; Component holder; OwnedArray<SlotComponent> slots;
    TextButton presetB, saveB { "SAVE" }, delB { "DELETE" }, abA { "A" }, abB { "B" }, rebuildB { "REBUILD CHAIN" }, learnB { String::fromUTF8 ("\xe2\x96\xb6  LEARN VOCAL") }, scB;
    ToggleButton lmB { "LEVEL MATCH" };
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> lmAtt;
    TextEditor nameEd;
    Slider inK, outK; std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> inAtt, outAtt;
    TextButton catB[3] { TextButton ("COMPRESSORS"), TextButton ("EQ"), TextButton ("DE-ESSER") };
    TextButton voiceB[3] { TextButton ("AUTO"), TextButton ("MALE"), TextButton ("FEMALE") };
    TextButton styleB[5] { TextButton ("RAP"), TextButton ("R&B"), TextButton ("POP"), TextButton ("ROCK"), TextButton ("BALLAD") };
    Component tray; OwnedArray<TrayCard> cards; int openCat = -1;
    String status; uint32 statusUntil = 0, learnShown = 0; String readout, plan;
    bool dirtyShown = false; int learnPhase = 0;

    void handleAsyncUpdate() override;
    void parameterChanged (const String&, float) override { triggerAsyncUpdate(); }
    void timerCallback() override;
    void layoutAll();
    void openTray (int cat);
    void presetMenu();
    void doLearn();
    void applyLearned();
    void rebuildFromChips();
    void makeReadout (const bh::VocalProfile& q);
    void makePlan();
    String voice() const { return proc.getProp ("voice", "AUTO"); }
    String style() const { return proc.getProp ("style", "RAP"); }
    void syncChips();
    bh::VocalProfile effectiveProfile() const;
    static void styleBtn (TextButton& b, bool hdr = false)
    {
        b.setColour (TextButton::buttonColourId, Colour (0xff2c2c30)); b.setColour (TextButton::buttonOnColourId, amber);
        b.setColour (TextButton::textColourOffId, Colour (0xffddd8cb)); b.setColour (TextButton::textColourOnId, Colours::black); juce::ignoreUnused (hdr);
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoomhypeEditor)
};

//==============================================================================
//                                   IMPLEMENTATION
//==============================================================================
inline SlotComponent::SlotComponent (BoomhypeEditor& e, BoomhypeProcessor& p, int slotIndex)
    : ed (e), proc (p), idx (slotIndex), type (p.typeAt (slotIndex)), style (p.styleAt (slotIndex)), def (&bh::getDef (type, style))
{
    for (auto* b : { &upB, &dnB, &delB, &bypB, &swapB })
    {
        b->setColour (TextButton::buttonColourId, Colour (0x99000000)); b->setColour (TextButton::textColourOffId, Colour (0xfff2eee2));
        addAndMakeVisible (b);
    }
    bypB.setClickingTogglesState (false);
    upB.onClick = [this] { proc.moveSlot (idx, idx - 1); ed.rebuild(); };
    dnB.onClick = [this] { proc.moveSlot (idx, idx + 1); ed.rebuild(); };
    delB.onClick = [this] { proc.removeSlot (idx); ed.rebuild(); };
    bypB.onClick = [this] { auto* q = proc.apvts.getParameter (bh::pidByp (idx)); q->beginChangeGesture(); q->setValueNotifyingHost (q->getValue() > 0.5f ? 0.f : 1.f); q->endChangeGesture(); };
    swapB.onClick = [this] { ed.swapMenu (idx); };
    swapB.setVisible (isComp());
    grip.idx = idx; addAndMakeVisible (grip);

    for (int j = 0; j < def->nKnobs(); ++j)
    {
        auto* k = knobs.add (new Knob());
        const auto& kd = def->knobs[(size_t) j];
        k->setSliderStyle (Slider::RotaryHorizontalVerticalDrag); k->setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        k->setRotaryParameters (rad (225.f), rad (495.f), true); k->setMouseDragSensitivity (200);
        k->steps = kd.steps.size(); k->getProperties().set ("dark", type == bh::Tube670 || type == bh::SnobTech || type == bh::ParamEQ || type == bh::Opto1B || type == bh::FET76 ? true : false);
        if (type == bh::Opto2A || type == bh::DeEsser || type == bh::AirEQ) k->getProperties().set ("dark", false);
        addAndMakeVisible (k);
        att.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, bh::pidKnob (idx, j), *k));
        k->setDoubleClickReturnValue (true, def->defNorm (j));
    }
    int gi = 0;
    for (auto& sw : def->sw)
    {
        swTitles.add (sw.title);
        for (int j = 0; j < sw.labels.size(); ++j)
        {
            auto* b = swBtns.add (new TextButton (sw.labels[j]));
            b->setColour (TextButton::buttonColourId, Colour (0xff3a3a3e)); b->setColour (TextButton::buttonOnColourId, amber);
            b->setColour (TextButton::textColourOffId, Colour (0xffddd8cb)); b->setColour (TextButton::textColourOnId, Colours::black);
            b->setClickingTogglesState (false);
            const bool radio = sw.radio; const int g = gi, jj = j;
            b->onClick = [this, g, jj, radio] { const int cur = getSw (g); setSw (g, radio ? jj : (cur ^ (1 << jj))); };
            addAndMakeVisible (b); swRefs.push_back ({ gi, j, radio });
        }
        ++gi;
    }
    if (def->styles.size() > 1)
    {
        for (size_t i = 0; i < def->styles.size(); ++i) styleBox.addItem (def->styles[i].name, (int) i + 1);
        styleBox.setSelectedId (style + 1, dontSendNotification);
        styleBox.setColour (ComboBox::backgroundColourId, Colour (0x66000000)); styleBox.setColour (ComboBox::textColourId, Colour (0xfff2eee2));
        styleBox.setColour (ComboBox::outlineColourId, Colour (0x55ffffff)); styleBox.setColour (ComboBox::arrowColourId, Colour (0xfff2eee2));
        styleBox.onChange = [this]
        {
            const int s = styleBox.getSelectedId() - 1;
            if (type == bh::SnobTech) proc.setModule (idx, type, s);
            else { auto* q = proc.apvts.getParameter (bh::pidStyle (idx)); q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 ((float) s)); q->endChangeGesture(); }
        };
        addAndMakeVisible (styleBox);
    }
}

inline bool SlotComponent::isDeB() const
{
    for (int s = 0; s < idx; ++s) { const int t = proc.typeAt (s); if (t >= bh::FET76 && t <= bh::Tube670) return true; }
    return false;
}
inline Colour SlotComponent::faceTop() const    { return type == bh::DeEsser ? Colour (isDeB() ? 0xff4a8c60 : 0xff2b6a45) : Colour (bh::getStyle (type, style).face); }
inline Colour SlotComponent::faceBottom() const { return type == bh::DeEsser ? faceTop().darker (0.2f) : Colour (bh::getStyle (type, style).face2); }
inline Colour SlotComponent::textCol() const    { return Colour (bh::getStyle (type, style).text); }

inline int SlotComponent::meterMode() const
{
    if (type == bh::FET76) { const int m = getSw (1); return m == 0 ? 0 : (m == 3 ? 2 : 1); }
    if (type == bh::Opto2A) return getSw (1) == 0 ? 0 : 1;
    if (type == bh::Opto1B) return getSw (0) == 0 ? 0 : 1;
    return 0;
}

inline void SlotComponent::resized()
{
    const int W = getWidth(), H = getHeight(); faceR = { 30, 0, W - 60, H };
    upB.setBounds (6, 28, 18, 16); dnB.setBounds (6, H - 44, 18, 16); grip.setBounds (4, H / 2 - 12, 22, 24);
    const int fx = faceR.getX(), rx = faceR.getRight() - 112;
    ledR = { fx + 12, 14, 12, 12 };
    styleBox.setBounds (fx + 12, 56, 128, 20); swapB.setBounds (fx + 12, def->styles.size() > 1 ? 82 : 60, 128, 20);
    delB.setBounds (rx, 10, 100, 20); bypB.setBounds (rx, 36, 100, 20);

    int vuW = def->vu ? 136 : 0; vuR = def->vu ? Rectangle<int> (rx - 8 - 128, (H - 112) / 2, 128, 112) : Rectangle<int>();
    // switch groups
    int nsw = (int) def->sw.size(), swW = 56, gx = (def->vu ? vuR.getX() - 6 : rx - 6) - nsw * swW;
    swTitleR.clear(); int bi = 0;
    for (int g = 0; g < nsw; ++g)
    {
        const int n = def->sw[(size_t) g].labels.size();
        const int top = 36 + (H - 156) / 2;
        swTitleR.add ({ gx + g * swW, top - 18, swW - 4, 14 });
        for (int j = 0; j < n; ++j) swBtns[bi++]->setBounds (gx + g * swW, top + j * 22, swW - 8, 19);
    }
    const int kx0 = fx + 156, kx1 = gx - 4, n = jmax (1, knobs.size());
    const int cw = jmin (110, (kx1 - kx0) / n);
    const int D = jlimit (34, 64, cw - 28);
    const int cy = H / 2 - 4;
    for (int j = 0; j < knobs.size(); ++j)
    {
        const int cx = kx0 + cw * j + cw / 2;
        int d = D;
        if (type == bh::Tube670 && (j == 0 || j == 1)) d = jmin (70, cw - 20);
        knobs[j]->setBounds (cx - d / 2, cy - d / 2, d, d);
    }
    juce::ignoreUnused (vuW);
}

inline void SlotComponent::mouseDown (const MouseEvent& e)
{
    if (ledR.expanded (6).contains (e.getPosition())) bypB.triggerClick();
}

inline void SlotComponent::tick()
{
    const bool byp = proc.bypassAt (idx);
    bypB.setColour (TextButton::buttonColourId, byp ? amber : Colour (0x99000000));
    bypB.setColour (TextButton::textColourOffId, byp ? Colours::black : Colour (0xfff2eee2));
    for (int i = 0; i < swBtns.size(); ++i)
    {
        const auto& r = swRefs[(size_t) i]; const int v = getSw (r.group);
        const bool on = r.radio ? (v == r.j) : ((v >> r.j) & 1);
        if (swBtns[i]->getToggleState() != on) swBtns[i]->setToggleState (on, dontSendNotification);
    }
    const float g = proc.slotGr[idx].load(), l = proc.slotLvl[idx].load();
    grDisp += 0.4f * (g - grDisp); lvlDisp += 0.25f * (l - lvlDisp);
    if (def->vu || type == bh::DeEsser || ! byp) repaint (vuR.expanded (4));
}

inline void SlotComponent::paint (Graphics& g)
{
    const int W = getWidth(), H = getHeight(); const bool byp = proc.bypassAt (idx);
    // rack ears
    for (int side = 0; side < 2; ++side)
    {
        Rectangle<float> e (side ? (float) W - 30 : 0.f, 0.f, 30.f, (float) H);
        g.setGradientFill (ColourGradient (Colour (0xff85888c), e.getX(), 0, Colour (0xff95989c), e.getRight(), 0, false)); g.fillRect (e);
        g.setColour (Colour (0xffc8cbcf).withAlpha (0.5f)); g.fillRect (e.getX() + 10, 0.f, 8.f, (float) H);
        g.setColour (Colours::black); for (float y : { 17.f, H - 17.f }) g.fillEllipse (e.getCentreX() - 5, y - 8, 10, 16);
    }
    auto f = faceR.toFloat();
    g.setGradientFill (ColourGradient (faceTop(), 0, 0, faceBottom(), 0, (float) H, false)); g.fillRect (f);
    if (type == bh::Opto1B) { g.setColour (Colour (0x30ffffff)); g.drawLine (f.getX(), 2, f.getRight(), 2, 2.f); }
    g.setColour (Colour (0x28ffffff)); g.drawLine (f.getX(), 1, f.getRight(), 1, 2.f);
    g.setColour (Colour (0x66000000)); g.drawLine (f.getX(), (float) H - 1, f.getRight(), (float) H - 1, 3.f);
    for (float x : { f.getX() + 8.f, f.getRight() - 8.f }) for (float y : { 8.f, (float) H - 8.f })
    { g.setGradientFill (ColourGradient (Colour (0xffececec), x - 2, y - 2, Colour (0xff4a4a4a), x + 4, y + 4, true)); g.fillEllipse (x - 5, y - 5, 10, 10); g.setColour (Colour (0xff1c1c1c)); g.drawLine (x - 3, y + 2, x + 3, y - 2, 1.4f); }

    // blue panel behind the VU on the 76 (blue stripe / rev F), red on the 2A
    if (def->vu && (type == bh::Opto2A || (type == bh::FET76 && (style == 0 || style == 2))))
    {
        const Colour c = type == bh::Opto2A ? Colour (0xffc22a1c) : Colour (0xff2a7fd0);
        g.setGradientFill (ColourGradient (c.darker (0.3f), (float) vuR.getX() - 4, 0, c, (float) vuR.getCentreX(), 0, true));
        g.fillRect ((float) vuR.getX() - 4, 0.f, (float) vuR.getWidth() + 8, (float) H);
    }
    const Colour tc = textCol();
    // name, LED, sub
    const auto led = Colour (bh::getStyle (type, style).led);
    g.setColour (byp ? Colour (0xff3a2a10) : led); g.fillEllipse (ledR.toFloat()); g.setColour (Colour (0x80000000)); g.drawEllipse (ledR.toFloat().expanded (1.5f), 2.f);
    String name = def->name, sub = def->sub;
    if (type == bh::DeEsser) { name = isDeB() ? "DE-ESSER B" : "DE-ESSER A"; sub = isDeB() ? "FINAL STAGE" : "PRE-COMPRESSION"; }
    g.setColour (def->nameCol ? Colour (def->nameCol) : tc); g.setFont (F (13.f, true));
    g.drawText (name, faceR.getX() + 30, 10, 130, 20, Justification::centredLeft);
    g.setColour (tc.withAlpha (0.85f)); g.setFont (F (8.f)); g.drawText (sub, faceR.getX() + 14, 32, 140, 12, Justification::centredLeft);
    g.drawText (String (idx + 1), 0, 0, 0, 0, Justification::left);
    g.setColour (Colour (0xff222222)); g.setFont (F (10.f, true)); g.drawText (String (idx + 1), 0, 48, 30, 14, Justification::centred);

    // knob scales, labels and value read-outs
    g.setColour (byp ? tc.withAlpha (0.4f) : tc);
    for (int j = 0; j < knobs.size(); ++j)
    {
        auto kb = knobs[j]->getBounds().toFloat(); const auto c = kb.getCentre(); const float r = kb.getWidth() * 0.5f + 4.f;
        const auto& kd = def->knobs[(size_t) j]; const int ns = kd.steps.size();
        const int nt = ns > 1 ? ns : 11;
        for (int i = 0; i < nt; ++i)
        {
            const float ang = 225.f + 270.f * (float) i / (float) (nt - 1);
            g.drawLine ({ onCircle (c, r, ang), onCircle (c, r + 4.f, ang) }, 1.2f);
            if (ns > 1 && (ns <= 6 || i % 2 == 0 || i == ns - 1))
            { g.setFont (F (jmax (6.f, kb.getWidth() / 7.5f))); auto p = onCircle (c, r + 10.f, ang); g.drawText (kd.steps[i], Rectangle<float> (p.x - 14, p.y - 5, 28, 10), Justification::centred); }
        }
        g.setFont (F (9.f, true)); g.drawText (kd.name, Rectangle<float> (c.x - 46, kb.getBottom() + 12, 92, 11), Justification::centred);
        g.setFont (F (9.f)); g.setColour ((byp ? tc.withAlpha (0.4f) : tc).withAlpha (0.8f));
        g.drawText (def->text (j, (float) knobs[j]->getValue()), Rectangle<float> (c.x - 46, kb.getBottom() + 23, 92, 11), Justification::centred);
        g.setColour (byp ? tc.withAlpha (0.4f) : tc);
    }
    g.setFont (F (8.f, true));
    for (int i = 0; i < swTitleR.size(); ++i) if (swTitles[i].isNotEmpty()) g.drawText (swTitles[i], swTitleR[i], Justification::centredLeft);

    if (def->vu)
    {
        const int m = meterMode(); const bool grMode = m == 0;
        float val = grDisp; if (! grMode)
        {
            const float ref = type == bh::FET76 ? (getSw (1) == 1 ? -14.f : -18.f) : type == bh::Opto2A ? (getSw (1) == 2 ? -8.f : -14.f) : -18.f;
            val = lvlDisp - ref + 3.f;
        }
        drawVU (g, vuR.toFloat(), grMode, byp ? 0.f : val, def->amber, byp || m == 2);
    }
    else if (type == bh::DeEsser)
    {
        g.setColour (Colour (0xff0a0a08)); auto r = Rectangle<float> ((float) faceR.getRight() - 112.f, 66.f, 100.f, 30.f); g.fillRoundedRectangle (r, 3.f);
        g.setColour (amberDim); g.setFont (FM (11.f));
        g.drawText ("GR -" + String (byp ? 0.f : grDisp, 1) + " dB", r.toNearestInt(), Justification::centred);
    }
}

inline void SlotComponent::paintOverChildren (Graphics& g)
{
    if (dragMode < 0) return;
    g.setColour (amber);
    if (dragMode == 1) { g.drawRect (faceR.reduced (3), 3); }
    else { const int y = dragMode == 0 ? 0 : getHeight() - 4; g.fillRect (0, y, getWidth(), 4); }
}

inline void SlotComponent::itemDragMove (const SourceDetails& d)
{
    const float fy = (float) d.localPosition.y / (float) getHeight();
    const bool isMove = d.description.toString().startsWith ("slot:");
    const int m = isMove ? (fy < 0.5f ? 0 : 2) : (fy < 0.25f ? 0 : fy > 0.75f ? 2 : 1);
    if (m != dragMode) { dragMode = m; repaint(); }
}
inline void SlotComponent::itemDropped (const SourceDetails& d)
{
    const int m = dragMode; dragMode = -1; repaint();
    ed.handleDrop (d.description.toString(), idx, m < 0 ? 1 : m);
}

//==============================================================================
inline void TrayCard::mouseDrag (const MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 6)
        if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
            if (! c->isDragAndDropActive()) c->startDragging ("mod:" + String (item.type) + ":" + String (item.style), this);
}
inline void TrayCard::mouseUp (const MouseEvent& e) { if (e.getDistanceFromDragStart() <= 6 && ! e.mouseWasDraggedSinceMouseDown()) ed.addModule (item.type, item.style); }

//==============================================================================
inline void BoomhypeEditor::Content::paint (Graphics& g)
{
    g.fillAll (Colour (0xff0d0d0d));
    auto plate = [&] (Rectangle<int> r) { g.setGradientFill (ColourGradient (Colour (0xff24211c), 0, (float) r.getY(), Colour (0xff14110d), 0, (float) r.getBottom(), false)); g.fillRoundedRectangle (r.toFloat(), 3.f); g.setColour (Colour (0xff3a3228)); g.drawRoundedRectangle (r.toFloat(), 3.f, 1.f); };
    plate ({ 8, 6, 1024, 46 }); plate ({ 8, 56, 1024, 40 }); plate ({ 8, 100 + o.vp.getHeight() + (o.openCat >= 0 ? 172 : 0) + 4, 1024, 46 });
    g.setColour (Colour (0xffe6e1d4)); g.setFont (F (20.f, true)); g.drawText ("BOOMHYPE", 24, 8, 140, 42, Justification::centredLeft);
    g.setColour (Colour (0xffa89f88)); g.setFont (F (10.f)); g.drawText ("VOCAL RACK", 150, 8, 100, 42, Justification::centredLeft);
    auto pn = o.proc.presetName() + (o.proc.isDirty() ? " *" : "");
    g.setColour (screen); g.fillRoundedRectangle (270.f, 14.f, 230.f, 28.f, 3.f); g.setColour (amberDim); g.setFont (F (11.f, true)); g.drawText (pn, 276, 14, 220, 28, Justification::centredLeft);
    // loudness meters
    auto bar = [&] (float x, float y, float db, const String& t)
    {
        g.setColour (screen); g.fillRect (x, y, 120.f, 10.f); const float f = jlimit (0.f, 1.f, (db + 60.f) / 60.f);
        g.setColour (db > -1.f ? Colour (0xffe03020) : db > -10.f ? amberDim : Colour (0xff37d67a)); g.fillRect (x, y, 120.f * f, 10.f);
        g.setColour (Colour (0xffa89f88)); g.setFont (F (8.f)); g.drawText (t, (int) x - 24, (int) y - 2, 22, 14, Justification::centredRight);
    };
    bar (540.f, 14.f, o.proc.inPeakDb.load(), "IN"); bar (540.f, 30.f, o.proc.outPeakDb.load(), "OUT");
    g.setColour (Colour (0xffa89f88)); g.setFont (F (8.f)); g.drawText ("IN TRIM", 700, 38, 44, 12, Justification::centred); g.drawText ("OUT TRIM", 750, 38, 50, 12, Justification::centred);
    // preset bar text
    g.setColour (Colour (0xffcfc8b6)); g.setFont (F (10.f, true)); g.drawText ("PRESET", 22, 56, 54, 40, Justification::centredLeft);
    const bool d = o.proc.isDirty();
    g.setColour (d ? amberDim : Colour (0xff8fd6a0)); g.setFont (F (10.f)); g.drawText (Time::getMillisecondCounter() < o.statusUntil ? o.status : (d ? "UNSAVED CHANGES" : String::fromUTF8 ("\xe2\x9c\x93 SAVED WITH PROJECT")), 740, 56, 280, 40, Justification::centredRight);

    // learning panel
    const int ly = 100 + o.vp.getHeight() + 4 + 52 + (o.openCat >= 0 ? 160 : 0);
    juce::ignoreUnused (ly);
    const int top = o.getHeight() > 0 ? 960 - 214 : 0;
    plate ({ 8, top, 1024, 206 });
    g.setColour (Colour (0xffcfc8b6)); g.setFont (F (10.f, true));
    const char* ph[] = { "IDLE", "LISTENING...", "ANALYZING...", "CHAIN BUILT", "NO VOCAL HEARD" };
    g.drawText ("VOCAL  LEARNING", 22, top + 6, 160, 16, Justification::centredLeft);
    g.setColour (amberDim); g.drawText (ph[o.learnPhase], 150, top + 6, 160, 16, Justification::centredLeft);
    auto box = [&] (Rectangle<int> r, const String& txt, Colour c) { g.setColour (screen); g.fillRoundedRectangle (r.toFloat(), 3.f); g.setColour (c); g.setFont (FM (10.f)); g.drawFittedText (txt, r.reduced (8, 5), Justification::topLeft, 12, 1.f); };
    box ({ 22, top + 114, 330, 86 }, o.readout.isEmpty() ? "Waiting for vocal.\nPlay your take, then press LEARN VOCAL.\n\nBEAT  send it to the sidechain input." : o.readout, amberDim);
    box ({ 368, top + 30, 650, 168 }, o.plan.isEmpty() ? "CHAIN PLAN\nPress LEARN VOCAL. The plugin listens to your vocal (and the beat on its sidechain input),\nworks out voice type, sibilance, resonance, dynamics and tone, then builds and sets the chain\nfor the style you picked." : o.plan, Colour (0xffd9b070));
}

inline BoomhypeEditor::BoomhypeEditor (BoomhypeProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (vp); vp.setViewedComponent (&holder, false); vp.setScrollBarsShown (true, false); vp.setScrollBarThickness (10);

    for (auto* b : { &presetB, &saveB, &delB, &abA, &abB, &rebuildB, &learnB, &scB, &catB[0], &catB[1], &catB[2], &voiceB[0], &voiceB[1], &voiceB[2], &styleB[0], &styleB[1], &styleB[2], &styleB[3], &styleB[4] })
    { styleBtn (*b); content.addAndMakeVisible (b); }
    for (auto* b : { &abA, &abB, &voiceB[0], &voiceB[1], &voiceB[2], &styleB[0], &styleB[1], &styleB[2], &styleB[3], &styleB[4], &scB, &catB[0], &catB[1], &catB[2] }) b->setClickingTogglesState (false);
    learnB.setColour (TextButton::buttonColourId, amber); learnB.setColour (TextButton::textColourOffId, Colours::black);

    nameEd.setTextToShowWhenEmpty ("name this chain", Colour (0xff6b5a38)); nameEd.setColour (TextEditor::backgroundColourId, screen);
    nameEd.setColour (TextEditor::textColourId, amberDim); nameEd.setColour (TextEditor::outlineColourId, Colour (0xff000000)); nameEd.setInputRestrictions (32);
    content.addAndMakeVisible (nameEd);
    lmB.setColour (ToggleButton::textColourId, Colour (0xffddd8cb)); content.addAndMakeVisible (lmB);
    lmAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "level_match", lmB);
    for (auto* s : { &inK, &outK }) { s->setSliderStyle (Slider::RotaryHorizontalVerticalDrag); s->setTextBoxStyle (Slider::NoTextBox, true, 0, 0); s->setRotaryParameters (rad (225.f), rad (495.f), true); s->getProperties().set ("dark", true); content.addAndMakeVisible (s); }
    inAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "in_trim", inK);
    outAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "out_trim", outK);
    inK.setDoubleClickReturnValue (true, 0.0); outK.setDoubleClickReturnValue (true, 0.0);

    presetB.onClick = [this] { presetMenu(); };
    saveB.onClick = [this] { auto n = nameEd.getText().trim(); if (n.isEmpty()) n = proc.presetName(); if (bh::defs().empty()) return; proc.saveUser (n); nameEd.clear(); flash ("SAVED: " + n.toUpperCase()); };
    delB.onClick = [this] { if (proc.userPresetNames().contains (proc.presetName())) { proc.deleteUser (proc.presetName()); proc.loadFactory ("SNOBBY VOCAL RACK"); rebuild(); } else flash ("FACTORY PRESETS CAN'T BE DELETED"); };
    abA.onClick = [this] { if (proc.abSlot != 0) { proc.switchAB(); rebuild(); } };
    abB.onClick = [this] { if (proc.abSlot != 1) { proc.switchAB(); rebuild(); } };
    for (int i = 0; i < 3; ++i) catB[i].onClick = [this, i] { openTray (openCat == i ? -1 : i); };
    for (int i = 0; i < 3; ++i) voiceB[i].onClick = [this, i] { proc.setProp ("voice", i == 0 ? "AUTO" : i == 1 ? "MALE" : "FEMALE"); rebuildFromChips(); };
    for (int i = 0; i < 5; ++i) styleB[i].onClick = [this, i] { proc.setProp ("style", styleB[i].getButtonText()); rebuildFromChips(); };
    learnB.onClick = [this] { doLearn(); };
    rebuildB.onClick = [this] { rebuildFromChips(); };
    scB.onClick = [] {};
    tray.setInterceptsMouseClicks (false, true); content.addChildComponent (tray);

    for (int s = 0; s < bh::kSlots; ++s) { proc.apvts.addParameterListener (bh::pidType (s), this); proc.apvts.addParameterListener (bh::pidStyle (s), this); }
    setResizable (true, true); setResizeLimits (780, 720, 1560, 1440); getConstrainer()->setFixedAspectRatio (1040.0 / 960.0);
    setSize (1040, 960);
    handleAsyncUpdate(); syncChips();
    startTimerHz (30);
}

inline BoomhypeEditor::~BoomhypeEditor()
{
    for (int s = 0; s < bh::kSlots; ++s) { proc.apvts.removeParameterListener (bh::pidType (s), this); proc.apvts.removeParameterListener (bh::pidStyle (s), this); }
    stopTimer(); cancelPendingUpdate(); setLookAndFeel (nullptr);
}

inline void BoomhypeEditor::resized()
{
    const float s = (float) getWidth() / 1040.f;
    content.setTransform (AffineTransform::scale (s)); content.setBounds (0, 0, 1040, 960); layoutAll();
}

inline void BoomhypeEditor::layoutAll()
{
    presetB.setBounds (74, 62, 236, 28); nameEd.setBounds (318, 62, 170, 28); saveB.setBounds (494, 62, 60, 28); delB.setBounds (558, 62, 64, 28);
    abA.setBounds (630, 62, 34, 28); abB.setBounds (668, 62, 34, 28); lmB.setBounds (556, 10, 120, 18); lmB.setVisible (true);
    lmB.setBounds (880, 14, 130, 28);
    inK.setBounds (700, 8, 34, 34); outK.setBounds (752, 8, 34, 34);
    const int learnTop = 960 - 214, trayH = openCat >= 0 ? 172 : 0;
    const int barY = learnTop - 54, vpH = barY - trayH - 100 - 4;
    vp.setBounds (8, 100, 1024, vpH);
    tray.setBounds (8, barY - trayH, 1024, trayH);
    for (int i = 0; i < 3; ++i) catB[i].setBounds (108 + i * 140, barY + 8, 132, 30);
    // learning panel
    voiceB[0].setBounds (22, learnTop + 26, 70, 24); voiceB[1].setBounds (96, learnTop + 26, 70, 24); voiceB[2].setBounds (170, learnTop + 26, 70, 24);
    for (int i = 0; i < 5; ++i) styleB[i].setBounds (22 + i * 66, learnTop + 54, 62, 24);
    scB.setBounds (250, learnTop + 26, 102, 24);
    learnB.setBounds (22, learnTop + 82, 214, 26);
    rebuildB.setBounds (244, learnTop + 82, 108, 26);
    vp.setBounds (8, 100, 1024, vpH);
    holder.setSize (1010, holder.getHeight());
}

inline void BoomhypeEditor::handleAsyncUpdate()
{
    vp.setViewedComponent (nullptr, false); slots.clear();
    int y = 0; const int n = proc.used();
    for (int s = 0; s < n; ++s)
    {
        auto* c = slots.add (new SlotComponent (*this, proc, s));
        holder.addAndMakeVisible (c); c->setBounds (0, y, 1010, c->heightNeeded()); y += c->heightNeeded() + 4;
    }
    holder.setSize (1010, jmax (y, 60));
    vp.setViewedComponent (&holder, false);
    if (cards.size() && openCat >= 0) openTray (openCat);
    repaint();
}

inline void BoomhypeEditor::openTray (int cat)
{
    openCat = cat; cards.clear(); tray.removeAllChildren();
    for (int i = 0; i < 3; ++i) catB[i].setToggleState (i == cat, dontSendNotification);
    tray.setVisible (cat >= 0); tray.setInterceptsMouseClicks (cat >= 0, true);
    if (cat >= 0)
    {
        const auto items = itemsFor (cat); const int cw = 196, ch = 62;
        for (int i = 0; i < items.size(); ++i)
        {
            auto* c = cards.add (new TrayCard (*this, items[i])); tray.addAndMakeVisible (c);
            c->setBounds (12 + (i % 5) * (cw + 8), 34 + (i / 5) * (ch + 8), cw, ch);
        }
    }
    layoutAll(); repaint();
}

inline void BoomhypeEditor::swapMenu (int slot)
{
    PopupMenu m; m.addSectionHeader ("REPLACE UNIT " + String (slot + 1) + " WITH");
    Array<Item> all = itemsFor (0); int id = 1;
    for (auto& it : all) m.addItem (id++, it.label);
    m.showMenuAsync (PopupMenu::Options(), [this, slot, all] (int r) { if (r > 0) { proc.setModule (slot, all[r - 1].type, all[r - 1].style); rebuild(); } });
}

inline void BoomhypeEditor::handleDrop (const String& d, int target, int mode)
{
    if (d.startsWith ("mod:"))
    {
        const auto t = StringArray::fromTokens (d, ":", ""); const int type = t[1].getIntValue(), st = t[2].getIntValue();
        if (mode == 1) proc.setModule (target, type, st);
        else if (! proc.insertModule (target + (mode == 2 ? 1 : 0), type, st)) flash ("RACK FULL (10 UNITS)");
    }
    else if (d.startsWith ("slot:"))
    {
        const int from = d.fromFirstOccurrenceOf (":", false, false).getIntValue();
        int to = target + (mode == 2 ? 1 : 0); if (from < to) --to;
        proc.moveSlot (from, to);
    }
    rebuild();
}

inline void BoomhypeEditor::presetMenu()
{
    PopupMenu m; m.addSectionHeader ("FACTORY");
    const auto& f = BoomhypeProcessor::factoryNames(); for (int i = 0; i < f.size(); ++i) m.addItem (1 + i, f[i], true, proc.presetName() == f[i]);
    const auto u = proc.userPresetNames();
    if (u.size()) { m.addSeparator(); m.addSectionHeader ("MY CHAINS"); for (int i = 0; i < u.size(); ++i) m.addItem (100 + i, u[i], true, proc.presetName() == u[i]); }
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetB), [this, f, u] (int r)
    {
        if (r <= 0) return;
        if (r < 100) { proc.loadFactory (f[r - 1]); proc.setProp ("voice", "AUTO"); proc.profile = {}; readout = plan = {}; learnPhase = 0; }
        else proc.loadUser (u[r - 100]);
        rebuild(); syncChips(); makePlan();
    });
}

//------------------------------------------------------------------------------
inline bh::VocalProfile BoomhypeEditor::effectiveProfile() const
{
    bh::VocalProfile q = proc.profile.valid ? proc.profile : bh::VocalProfile {};
    if (voice() == "MALE") q.male = true; else if (voice() == "FEMALE") q.male = false;
    return q;
}

inline void BoomhypeEditor::syncChips()
{
    for (int i = 0; i < 3; ++i) voiceB[i].setToggleState (voice() == voiceB[i].getButtonText(), dontSendNotification);
    for (int i = 0; i < 5; ++i) styleB[i].setToggleState (style() == styleB[i].getButtonText(), dontSendNotification);
}

inline void BoomhypeEditor::rebuildFromChips()
{
    const auto q = effectiveProfile();
    proc.buildChain (style(), q, proc.scLevelDb.load() > -80.f);
    if (proc.profile.valid) makeReadout (q);
    syncChips(); makePlan(); rebuild();
}

inline void BoomhypeEditor::doLearn()
{
    if (proc.learnState.load() == BoomhypeProcessor::Capturing)
    {
        if (proc.learnPos.load() > (int) (proc.getSampleRate() * 2.0)) proc.learnState = BoomhypeProcessor::Ready;   // FINISH early
        else flash ("KEEP PLAYING, NEED A FEW SECONDS OF VOCAL");
        return;
    }
    proc.startLearn(); learnPhase = 1; readout = {};
}

inline void BoomhypeEditor::makeReadout (const bh::VocalProfile& q)
{
    auto khz = [] (float v) { return String (v, 1) + " kHz"; };
    String t;
    t << "VOICE      " << (q.male ? "MALE" : "FEMALE") << " . F0 ~ " << (int) q.f0 << " Hz\n"
      << "SIBILANCE  " << String (q.sibLo, 1) << "-" << String (q.sibHi, 1) << " kHz . PEAK " << khz (q.sibPk) << "\n"
      << "DYNAMICS   " << (q.dyn >= 15.f ? "WIDE" : "MEDIUM") << " . " << (int) q.dyn << " dB\n"
      << "SPIKE      " << (q.spikeFound ? String ((int) q.spike) + " Hz" : String ("NONE FOUND")) << "\n"
      << "TONE       " << (q.dull ? "DULL, NEEDS AIR" : "BRIGHT ENOUGH") << "\n"
      << "ROOM       " << (q.room ? "AMBIENCE, SHORT REVERB" : "DRY") << "\n"
      << "BEAT       " << (q.beatValid ? String ((int) q.bpm) + " BPM . BASS " + String ((int) q.bassHz) + " Hz" : String ("NOT CONNECTED"));
    readout = t;
}

inline void BoomhypeEditor::makePlan()
{
    String t = "CHAIN PLAN . " + style() + " . " + (effectiveProfile().male ? "MALE" : "FEMALE") + "\n";
    for (int s = 0; s < proc.used(); ++s)
    {
        const auto& d = bh::getDef (proc.typeAt (s), proc.styleAt (s)); String line = String (s + 1) + "  " + d.tag;
        if (d.type == bh::DeEsser) line = String (s + 1) + "  DE-ESS " + String (s > 1 ? "B" : "A");
        if (d.type == bh::FET76) line += " " + d.styles[(size_t) proc.styleAt (s)].name.upToFirstOccurrenceOf (" ", false, false);
        line = line.paddedRight (' ', 26) + "";
        const int n = jmin (3, d.nKnobs());
        for (int j = 0; j < n; ++j) line << " " << d.knobs[(size_t) j].name << " " << d.text (j, proc.apvts.getRawParameterValue (bh::pidKnob (s, j))->load());
        t << line << "\n";
    }
    t << "\nNot built yet: saturation, reverb and delay sends. (Planned.)";
    plan = t;
}

inline void BoomhypeEditor::applyLearned()
{
    const auto& q = proc.profile;
    if (! q.valid) { learnPhase = 4; readout = "NO VOCAL HEARD.\nPlay your take (and the beat on the\nsidechain input), then press LEARN VOCAL."; proc.learnState = BoomhypeProcessor::Idle; return; }
    rebuildFromChips(); learnPhase = 3;
    proc.learnState = BoomhypeProcessor::Idle;
}

inline void BoomhypeEditor::timerCallback()
{
    const int ls = proc.learnState.load();
    if (ls == BoomhypeProcessor::Ready) { learnPhase = 2; proc.startAnalysis(); }
    else if (ls == BoomhypeProcessor::Done) applyLearned();
    else if (ls == BoomhypeProcessor::Capturing)
    {
        learnPhase = 1; const float secs = (float) proc.learnPos.load() / (float) jmax (1.0, proc.getSampleRate());
        readout = "LISTENING...  " + String (secs, 1) + " s  (play the whole take)\nINPUT PEAK  " + String (proc.inPeakDb.load(), 1) + " dB\n\nPress FINISH when the take is done,\nor it stops by itself after 12 s.";
    }
    learnB.setButtonText (ls == BoomhypeProcessor::Capturing ? "FINISH" : String::fromUTF8 ("\xe2\x96\xb6  LEARN VOCAL"));
    const bool sc = proc.scLevelDb.load() > -80.f;
    scB.setButtonText (sc ? "BEAT: LIVE" : "BEAT: NONE"); scB.setToggleState (sc, dontSendNotification);
    presetB.setButtonText (proc.presetName() + (proc.isDirty() ? " *" : "") + "  v");
    abA.setToggleState (proc.abSlot == 0, dontSendNotification); abB.setToggleState (proc.abSlot == 1, dontSendNotification);
    for (auto* s : slots) s->tick();
    content.repaint();
}
} // namespace ui

using ui::BoomhypeEditor;

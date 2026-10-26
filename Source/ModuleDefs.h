#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <vector>

// Everything the rack knows about its modules lives here: knob ranges, switch
// labels and faceplate colours. The audio engines and the editor both read it.
namespace bh
{
constexpr int kSlots = 10;   // units in the rack
constexpr int kKnobs = 8;    // generic knob parameters per unit
constexpr int kSw    = 3;    // generic switch parameters per unit

enum ModType { Empty = 0, ParamEQ, DeEsser, FET76, Opto2A, Opto1B, Tube670, SnobTech, AirEQ, NumTypes };

struct KnobDef
{
    juce::String name;
    float mn = 0, mx = 1, def = 0;
    juce::String unit;
    int dec = 0;
    bool log = false;
    juce::StringArray steps;           // non-empty = stepped knob (value is an index)
    bool dark = true;                  // black fluted knob vs silver
};

struct SwDef { juce::String title; juce::StringArray labels; bool radio = false; int def = 0; };

struct Style { juce::String name; juce::uint32 face, face2, text, led; };

struct ModDef
{
    int type = Empty;
    juce::String name, sub, tag;
    std::vector<KnobDef> knobs;
    std::vector<SwDef> sw;
    std::vector<Style> styles;
    bool vu = false, amber = false;
    juce::uint32 nameCol = 0;

    int nKnobs() const { return (int) knobs.size(); }

    float real (int j, float n) const
    {
        const auto& k = knobs[(size_t) j];
        n = juce::jlimit (0.f, 1.f, n);
        if (! k.steps.isEmpty()) return std::round (n * (float) (k.steps.size() - 1));
        if (k.log) return k.mn * std::pow (k.mx / k.mn, n);
        return k.mn + (k.mx - k.mn) * n;
    }
    float norm (int j, float v) const
    {
        const auto& k = knobs[(size_t) j];
        if (! k.steps.isEmpty()) return k.steps.size() > 1 ? v / (float) (k.steps.size() - 1) : 0.f;
        if (k.log) return std::log (juce::jlimit (k.mn, k.mx, v) / k.mn) / std::log (k.mx / k.mn);
        return (juce::jlimit (k.mn, k.mx, v) - k.mn) / (k.mx - k.mn);
    }
    float defNorm (int j) const { return norm (j, knobs[(size_t) j].def); }

    juce::String text (int j, float n) const
    {
        const auto& k = knobs[(size_t) j];
        const float v = real (j, n);
        if (! k.steps.isEmpty()) return k.steps[juce::jlimit (0, k.steps.size() - 1, (int) v)];
        if (k.unit == " Hz" && v >= 1000.f) return juce::String (v / 1000.f, 2) + " kHz";
        return (k.mn < 0 && v > 0 ? "+" : "") + juce::String (v, k.dec) + k.unit;
    }
};

inline float parseHz (const juce::String& s)
{
    return s.endsWithIgnoreCase ("k") ? s.dropLastCharacters (1).getFloatValue() * 1000.f : s.getFloatValue();
}

namespace detail
{
    inline KnobDef K (const char* n, float mn, float mx, float d, const char* u = "", int dec = 0, bool lg = false)
    { KnobDef k; k.name = n; k.mn = mn; k.mx = mx; k.def = d; k.unit = u; k.dec = dec; k.log = lg; return k; }

    inline KnobDef S (const char* n, const char* steps, int d)
    { KnobDef k; k.name = n; k.steps = juce::StringArray::fromTokens (steps, "|", ""); k.mn = 0; k.mx = (float) (k.steps.size() - 1); k.def = (float) d; return k; }

    inline SwDef W (const char* t, const char* labels, bool radio, int d)
    { SwDef s; s.title = t; s.labels = juce::StringArray::fromTokens (labels, "|", ""); s.radio = radio; s.def = d; return s; }

    inline Style St (const char* n, juce::uint32 f, juce::uint32 f2, juce::uint32 t, juce::uint32 led)
    { return { n, f, f2, t, led }; }

    inline std::vector<ModDef> build()
    {
        std::vector<ModDef> d ((size_t) NumTypes + 1);

        auto& e = d[Empty]; e.type = Empty; e.name = "EMPTY";

        auto& eq = d[ParamEQ];
        eq.type = ParamEQ; eq.name = "BOOMHYPE-EQ"; eq.sub = "PARAMETRIC EQ"; eq.tag = "PARAMETRIC EQ";
        eq.knobs = { K ("LOW CUT", 20, 200, 90, " Hz", 0, true), K ("MUD", -12, 12, -3, " dB", 1), K ("MUD FREQ", 100, 2000, 300, " Hz", 0, true),
                     K ("PRESENCE", -12, 12, 2, " dB", 1), K ("PRES FREQ", 1000, 8000, 3500, " Hz", 0, true) };
        eq.sw = { W ("", "HPF IN|MUD IN|PRES IN", false, 7) };
        eq.styles = { St ("BLACK", 0xff1c1c1c, 0xff141414, 0xffe6e1d4, 0xffe8a040) };

        auto& de = d[DeEsser];
        de.type = DeEsser; de.name = "DE-ESSER"; de.sub = ""; de.tag = "DE-ESSER";
        de.knobs = { K ("FREQ", 2000, 12000, 6500, " Hz", 0, true), K ("REDUCE", -12, 0, -4, " dB", 1), K ("RANGE", 0, 20, 8, " dB", 1) };
        de.sw = { W ("", "LISTEN|WIDE", false, 0) };
        de.styles = { St ("GREEN", 0xff2b6a45, 0xff245a3b, 0xffeef6f0, 0xffe8a040) };

        auto& f = d[FET76];
        f.type = FET76; f.name = "BOOMHYPE-76"; f.sub = "LIMITING AMPLIFIER"; f.tag = "76 FET"; f.vu = true;
        f.knobs = { K ("INPUT", -20, 20, 6, " dB", 1), K ("MIX", 0, 100, 100, "%", 0), K ("OUTPUT", -20, 20, 0, " dB", 1),
                    K ("ATTACK", 1, 7, 3, "", 1), K ("RELEASE", 1, 7, 6, "", 1) };
        f.sw = { W ("RATIO", "20|12|8|4|ALL", true, 3), W ("METER", "GR|+8|+4|OFF", true, 0) };
        f.styles = { St ("BLUE STRIPE - REV A", 0xffc9ccd0, 0xffaeb1b6, 0xff1a1a1a, 0xffe03020),
                     St ("BLACKFACE - REV D",  0xff1d1d1d, 0xff141414, 0xffe6e1d4, 0xffe8a040),
                     St ("BLACKFACE - REV F",  0xff2e2e30, 0xff222224, 0xffe6e1d4, 0xffe8a040),
                     St ("SILVERFACE - REV H", 0xffdcdee1, 0xffbfc2c6, 0xff1a1a1a, 0xffe03020),
                     St ("LN - LOW NOISE",     0xff4b3446, 0xff3c2a38, 0xffe6e1d4, 0xffe8a040) };

        auto& a = d[Opto2A];
        a.type = Opto2A; a.name = "BOOMHYPE-2A"; a.sub = "OPTO LEVELER"; a.tag = "2A OPTO"; a.vu = true;
        a.knobs = { K ("PEAK REDUCTION", 0, 100, 45), K ("GAIN", -10, 20, 4, " dB", 1), K ("HF EMPH", 0, 10, 0, "", 1) };
        a.sw = { W ("", "LIMIT", false, 0), W ("METER", "GR|+4|+10", true, 0) };
        a.styles = { St ("STANDARD SILVER", 0xffb9bcc0, 0xffa2a5a9, 0xff1a1a1a, 0xffe03020),
                     St ("SILVER FACE",     0xffeceef0, 0xffb4b7bb, 0xff1a1a1a, 0xffe03020),
                     St ("GRAY FACE",       0xff6b7075, 0xff5a5e63, 0xfff0f0f0, 0xffe03020) };

        auto& b = d[Opto1B];
        b.type = Opto1B; b.name = "BOOMHYPE-1B"; b.sub = "CL1B-STYLE OPTO"; b.tag = "1B OPTO"; b.vu = true;
        b.knobs = { K ("THRESHOLD", -40, 0, -14, " dB", 1), K ("RATIO", 2, 10, 3, ":1", 1), K ("ATTACK", 1, 50, 10, " ms", 1, true),
                    K ("RELEASE", 50, 1000, 200, " ms", 0, true), K ("OUTPUT", -10, 20, 5, " dB", 1) };
        b.sw = { W ("METER", "GR|OUT", true, 0) };
        b.styles = { St ("DEEP NAVY", 0xff143272, 0xff0c2150, 0xfff2f4f8, 0xffff3b2a) };

        auto& t = d[Tube670];
        t.type = Tube670; t.name = "BOOMHYPE-670"; t.sub = "TUBE LIMITER - FINAL"; t.tag = "670 TUBE"; t.vu = true; t.amber = true;
        t.nameCol = 0xffe8402e;
        t.knobs = { K ("INPUT GAIN", 0, 20, 2, " dB", 1), K ("AC THRESHOLD", 0, 10, 5, "", 1), S ("DC THRESHOLD", "1|2|3|4", 1),
                    S ("TIME CONST", "1|2|3|4|5|6", 2), S ("SC FILTER", "OFF|50 Hz|100 Hz|200 Hz|350 Hz", 0), K ("OUTPUT", -10, 10, 0, " dB", 1) };
        t.styles = { St ("MATTE BLACK", 0xff141414, 0xff0c0c0c, 0xfff1efe9, 0xffff3b2a) };

        auto& s = d[SnobTech];
        s.type = SnobTech; s.name = "SNOBTECH EQ"; s.sub = "PROGRAM EQUALIZER"; s.tag = "SNOBTECH"; s.nameCol = 0xfff1efe6;
        s.knobs = { S ("LOW FREQ", "200|300|500|700|1k", 0), K ("LOW PEAK", 0, 10, 0, "", 1),
                    S ("DIP FREQ", "200|300|500|700|1k|1.5k|2k|3k|4k|5k|7k", 1), K ("DIP", 0, 10, 0, "", 1),
                    S ("HIGH FREQ", "1.5k|2k|3k|4k|5k", 2), K ("HIGH PEAK", 0, 10, 0, "", 1) };
        s.sw = { W ("", "IN", false, 1) };
        s.styles = { St ("MID-RANGE", 0xff3d4c68, 0xff33405a, 0xfff1efe6, 0xffff3b2a),
                     St ("LOW / HIGH", 0xff394957, 0xff2f3e4a, 0xfff1efe6, 0xffff3b2a) };
        d[NumTypes] = s;                       // slot NumTypes = SnobTech "program" (low/high) variant
        auto& sp = d[NumTypes];
        sp.sub = "PROGRAM EQUALIZER";
        sp.knobs = { S ("LOW FREQ", "20|30|60|100", 2), K ("LOW BOOST", 0, 10, 0, "", 1), K ("LOW ATTEN", 0, 10, 0, "", 1),
                     S ("HI FREQ", "3k|4k|5k|8k|10k|12k|16k", 4), K ("BANDWIDTH", 0, 10, 5, "", 1), K ("HI BOOST", 0, 10, 0, "", 1),
                     S ("ATTEN SEL", "5k|10k|20k", 1), K ("HI ATTEN", 0, 10, 0, "", 1) };

        auto& ar = d[AirEQ];
        ar.type = AirEQ; ar.name = "BOOMHYPE-AIR"; ar.sub = "AIR EQ - MAAG-STYLE"; ar.tag = "AIR EQ";
        ar.knobs = { K ("AIR GAIN", 0, 10, 2.5f, " dB", 1), S ("AIR FREQ", "OFF|2.5k|5k|10k|20k|40k", 3), K ("LEVEL TRIM", -10, 10, 0, " dB", 1) };
        ar.sw = { W ("", "IN|WIDE", false, 1) };
        ar.styles = { St ("AZURE", 0xff2aa6ea, 0xff1790d8, 0xfff4f8fc, 0xffe8a040) };
        return d;
    }
}

inline const std::vector<ModDef>& defs() { static const auto d = detail::build(); return d; }

// SnobTech style 1 (low/high) uses its own knob set, stored at index NumTypes
inline const ModDef& getDef (int type, int style = 0)
{
    if (type == SnobTech && style == 1) return defs()[(size_t) NumTypes];
    return defs()[(size_t) juce::jlimit (0, (int) NumTypes - 1, type)];
}

inline const Style& getStyle (int type, int style)
{
    const auto& d = defs()[(size_t) juce::jlimit (0, (int) NumTypes - 1, type)];
    return d.styles.empty() ? defs()[ParamEQ].styles[0] : d.styles[(size_t) juce::jlimit (0, (int) d.styles.size() - 1, style)];
}

// parameter ids
inline juce::String pidType  (int s)        { return "s" + juce::String (s) + "_type"; }
inline juce::String pidStyle (int s)        { return "s" + juce::String (s) + "_style"; }
inline juce::String pidByp   (int s)        { return "s" + juce::String (s) + "_byp"; }
inline juce::String pidKnob  (int s, int j) { return "s" + juce::String (s) + "_k" + juce::String (j); }
inline juce::String pidSw    (int s, int j) { return "s" + juce::String (s) + "_w" + juce::String (j); }
} // namespace bh

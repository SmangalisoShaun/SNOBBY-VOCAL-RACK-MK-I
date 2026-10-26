# BOOMHYPE Vocal Rack (one plugin, VST3)

One plugin with a whole vocal rack inside it. Default chain (preset **SNOBBY VOCAL RACK**):
Parametric EQ > De-Esser A > 76 (1176) > 2A (LA-2A) > SNOBTECH EQ (Pultec) > De-Esser B > 670 (glue)

Modules: Parametric EQ, De-Esser, 76 FET (5 faceplates), 2A Opto (3), 1B Opto (CL1B-style), 670 Tube Limiter,
SNOBTECH EQ (mid-range + low/high), Air EQ (Maag-style).

## Build
- **Easiest (Windows VST3 for FL Studio):** push this folder to GitHub. Actions builds it. Download the
  artifact `BOOMHYPE-Vocal-Rack-VST3-Windows`, copy `BOOMHYPE Vocal Rack.vst3` to
  `C:\Program Files\Common Files\VST3\`, then rescan plugins in FL Studio.
- **Linux / Codespaces test build:** `./setup.sh` once, then `./build.sh`.

## In FL Studio
- Put it on the vocal's mixer track. Route the beat's track to it as a **sidechain**
  (beat track > right-click the send arrow on the vocal track > *Sidechain to this track*). The beat is
  only listened to, never mixed in.
- **LEARN VOCAL**: press it, play the whole take, press FINISH. It measures voice type, sibilance, resonance,
  dynamics, tone, room (and tempo / bass of the beat) and sets every knob for the style you picked
  (RAP, R&B, POP, ROCK, BALLAD). Style / voice chips rebuild the chain.
- **Presets**: SAVE stores your chain in `Documents/BOOMHYPE/Vocal Chains`. The project file itself keeps the
  full chain, every knob and which preset was loaded, so reopening the project restores them (a `*` means changed since load).
- Drag compressors from the COMPRESSORS tray onto a unit to swap, or onto its top/bottom edge to insert.
  Drag the grip on the left ear to reorder.

## Notes
- All control values are saved as normalised host parameters named "Unit N Knob M", so automation in FL
  works but the names are generic.
- DSP is modelled on how each unit behaves (FET attack/release + saturation, opto two-stage release, variable-mu
  670 curve, Pultec shelf/cut trick). It is a model, not a circuit emulation.

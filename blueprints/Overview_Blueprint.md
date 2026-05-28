# AndroidVoice - Technical Design Document
*nth_tech | prototype_v1.1 | authored by Retkeren*

---

## Changelog

### v1.1 - Hung Note Fix
- Introduced explicit three-state voice lifecycle: `Inactive`, `Playing`, `Releasing`
- `getVoiceForNote()` now only matches `Playing` voices - releasing voices are invisible to note routing
- Added `forceOff()` for hard voice kill on allNotesOff and voice stealing
- Added `isPlayingNote(int)` predicate to replace raw `note ==` comparisons
- Fixed velocity=0 noteOn treated as noteOff (MIDI spec compliance)
- Fixed duplicate noteOn for same pitch now releases old voice cleanly before reassigning
- Implemented sample-accurate MIDI splitting - block is sliced at each event timestamp, eliminating same-block noteOn/noteOff collisions
- Extracted `renderVoices()` helper in processor to support sample-accurate rendering
- Voice stealing now prefers releasing voices before hard-stealing playing ones

---

## Overview

AndroidVoice is a polyphonic wavetable synthesizer VST3/Standalone plugin built in C++ using the JUCE framework. It specializes in producing android/vocal-machine timbres through a combination of formant-shaped wavetable synthesis, randomized LFO modulation of table position, and a resonant filter with per-voice cutoff drift.

The architecture is intentionally minimal for v1 - pure sound engine, no GUI, no saved parameters. The goal was to produce the android vocal character and validate the signal chain before adding surface controls.

---

## System Architecture

```
MIDI Input
    │
    ▼
AndroidVoiceProcessor          (JUCE AudioProcessor -- entry point)
    │
    ├── buildAndroidWavetable()    (runs once at construction)
    │       └── std::array<float, 2048>   (shared wavetable data)
    │
    └── voices[8]              (polyphonic voice pool)
            │
            ▼
        AndroidVoice           (per-voice processing unit)
            │
            ├── State machine          (Inactive / Playing / Releasing)
            │
            ├── WavetableOscillator    (sample generation + LFO)
            │       ├── wavetable[]        (read-only reference)
            │       ├── phaseIncrement     (pitch)
            │       └── LFO                (table position modulator)
            │
            ├── StateVariableTPTFilter (resonant lowpass)
            │       └── drifting cutoff    (per-voice organic movement)
            │
            └── ADSR Envelope          (amplitude shaping)
                    └── output sample
                            │
                            ▼
                    mono mix buffer
                            │
                            ▼
                    stereo output channels
```

---

## Module Descriptions

---

### 1. `buildAndroidWavetable()` - Wavetable Factory
**File:** `WavetableOscillator.h`

**Purpose:**
Constructs a single-cycle waveform stored as a 2048-sample float array. This waveform is the tonal raw material for every voice. It is built once at plugin construction and shared as a read-only reference across all voices.

**How it works:**
The wavetable is built by summing 10 harmonics with amplitude values shaped to mimic the formant peaks of a sung vowel. Harmonics 2-3 (F1 region) and 6-7 (F2 region) are boosted to bake vowel character into the raw waveform.

**Harmonic map:**
```
Harmonic 1  -> 0.50  (fundamental, moderate)
Harmonic 2  -> 0.90  (F1 approach)
Harmonic 3  -> 1.00  (F1 peak -- loudest harmonic)
Harmonic 4  -> 0.85  (F2 approach)
Harmonic 5  -> 0.40  (inter-formant dip)
Harmonic 6  -> 0.75  (F2 region)
Harmonic 7  -> 0.80  (F2 peak)
Harmonic 8  -> 0.60  (upper harmonic body)
Harmonics 9-10 -> 0.25, 0.15  (air/presence, rolled off)
```

After summation the table is normalized so the peak sample equals 1.0.

**Key constant:**
`WAVETABLE_SIZE = 2048` - power of 2 enables efficient modulo wrapping during playback.

---

### 2. `WavetableOscillator` - Sample Generator
**File:** `WavetableOscillator.h`

**Purpose:**
Reads through the wavetable at a rate determined by the target pitch, producing one audio sample per call to `getNextSample()`. An LFO continuously offsets the read position, making the waveform morph rather than repeat identically each cycle.

**Core members:**
| Member | Role |
|---|---|
| `currentIndex` | Current read position in the wavetable (float for sub-sample precision) |
| `phaseIncrement` | How many table samples to advance per audio sample -- set by pitch |
| `lfoPhase` | Current LFO phase (0.0 to 1.0) |
| `lfoPhaseIncrement` | LFO advance rate per audio sample |
| `lfoRate` | LFO frequency in Hz (randomized 0.3-4.5 Hz per note) |
| `lfoDepth` | Maximum table offset in samples (randomized 5-35% of table size) |

**Pitch calculation:**
```
phaseIncrement = WAVETABLE_SIZE x frequency / sampleRate
```

**LFO modulation:**
Each audio sample the LFO produces a sine value (-1.0 to +1.0) multiplied by `lfoDepth` to get a table offset. Added to `currentIndex` before lookup, sweeping read position forward and backward. Result: continuous timbral movement, the waveform never repeats exactly.

**Linear interpolation:**
Reads two adjacent samples and blends proportionally to avoid aliasing at all pitches.

**Randomization:**
`randomizeLFO()` called on every `noteOn`, pulling new values for `lfoRate` and `lfoDepth` from a Mersenne Twister RNG. Every voice has a different modulation character.

---

### 3. `AndroidVoice` - Polyphonic Voice Unit
**File:** `AndroidVoice.h`

**Purpose:**
Wraps one oscillator, one filter, and one envelope into a single self-contained voice. The processor maintains a pool of 8 of these.

**Signal chain per voice:**
```
WavetableOscillator -> StateVariableTPTFilter -> ADSR Envelope -> output
```

**State machine (v1.1):**
```
Inactive --[noteOn]--> Playing --[noteOff]--> Releasing --[envelope done]--> Inactive
                                                        --[forceOff]------> Inactive
Inactive --[forceOff]--> Inactive  (no-op, safe to call anytime)
```

- `Playing` - voice is active and responding to note routing
- `Releasing` - envelope in release tail, invisible to `getVoiceForNote()`
- `Inactive` - voice is free, available for reuse

**Filter:**
`StateVariableTPTFilter` in lowpass mode. Resonance fixed at 0.7. Cutoff drifts every 512 samples within +/-200Hz of current value, clamped 400Hz-5000Hz, interpolated at 0.001 coefficient per sample.

**Envelope (ADSR):**
```
Attack  = 15ms
Decay   = 200ms
Sustain = 70%
Release = 400ms
```

**Voice lifecycle:**
1. `noteOn(midiNote, velocity)` -- sets state to Playing, resets envelope, randomizes LFO, sets filter cutoff
2. `process(buffer, numSamples)` -- fills output buffer, transitions to Inactive when envelope finishes
3. `noteOff()` -- transitions Playing -> Releasing, triggers envelope release (no-op if already Releasing)
4. `forceOff()` -- hard kill, immediate Inactive transition regardless of state

---

### 4. `AndroidVoiceProcessor` - Plugin Entry Point
**File:** `PluginProcessor.h` / `PluginProcessor.cpp`

**Purpose:**
The JUCE `AudioProcessor` subclass. Owns the wavetable and voice pool, handles MIDI routing, mixes voice outputs, delivers audio to host.

**`processBlock()` - sample-accurate MIDI (v1.1):**
Rather than processing all MIDI events then all audio, the block is now split at each event timestamp:
```
[audio 0..eventA] -> [handle eventA] -> [audio eventA..eventB] -> [handle eventB] -> ...
```
This eliminates same-block noteOn/noteOff collisions that previously caused hung notes.

**Voice allocation:**
`getFreeVoice()` priority order:
1. Truly Inactive voice (ideal)
2. Releasing voice (already fading, steal acceptable)
3. Hard-steal voices[0] via `forceOff()` (last resort)

`getVoiceForNote(int note)` only matches voices in `Playing` state. Releasing voices are intentionally excluded from note routing.

**Output gain:**
Each voice scaled by `0.3f` before accumulation. With 8 voices this keeps summed output below clipping at approximately 2.4x peak.

---

## Data Flow Summary

```
1. Plugin loads -> wavetable built once (2048 floats, formant-shaped harmonics)

2. MIDI noteOn received ->
   - Any existing Playing voice for this note released first
   - Free voice located (Inactive preferred, Releasing acceptable, steal as last resort)
   - State set to Playing
   - Pitch, LFO, filter cutoff initialized
   - ADSR triggered

3. Per audio sample (inside processBlock) ->
   - LFO advances -> generates offset value
   - Wavetable read at (currentIndex + lfoOffset) with interpolation
   - Sample passes through resonant lowpass (cutoff drifting slowly)
   - Multiplied by ADSR envelope value and velocity
   - Accumulated into mono mix buffer

4. MIDI noteOff received ->
   - Voice state: Playing -> Releasing
   - ADSR enters release phase
   - Voice invisible to new note routing
   - Voice continues processing until envelope reaches zero
   - Voice state: Releasing -> Inactive

5. Mono buffer copied to stereo output -> DAW receives audio
```

---

## Known Issues (v1.1)

- **No GUI** -- all parameters hardcoded, recompile to change
- **No parameter automation** -- no AudioProcessorValueTreeState
- **Mono output** -- both stereo channels receive identical signal
- **Voice stealing is simple** -- steals voices[0] when all 8 active; no age or amplitude awareness

---

## Planned v2 Features

- `AudioProcessorValueTreeState` -- expose LFO rate, LFO depth, filter cutoff, resonance, ADSR as automatable parameters
- GUI -- dark panel, blue/orange color scheme, knobs for all exposed parameters, live oscilloscope showing wavetable morph in real time
- Second wavetable slot (sine/saw) with morphing between tables via knob
- True stereo -- pan voices slightly across field for width
- Wavefolder/soft clip distortion stage for more android grit
- Smarter voice stealing -- prefer quietest or oldest releasing voice

---

*Built: May 2026 | JUCE 7.0.9 | C++17 | VST3*
*v1.1 patch: May 2026*

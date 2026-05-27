# AndroidVoice — Technical Design Document
*nth_tech | prototype_v1 | authored by Retkeren*

---

## Overview

AndroidVoice is a polyphonic wavetable synthesizer VST3/Standalone plugin built in C++ using the JUCE framework. It specializes in producing android/vocal-machine timbres through a combination of formant-shaped wavetable synthesis, randomized LFO modulation of table position, and a resonant filter with per-voice cutoff drift.

The architecture is intentionally minimal for v1 — pure sound engine, no GUI, no saved parameters. The goal was to produce the android vocal character and validate the signal chain before adding surface controls.

---

## System Architecture

```
MIDI Input
    │
    ▼
AndroidVoiceProcessor          (JUCE AudioProcessor — entry point)
    │
    ├── buildAndroidWavetable()    (runs once at construction)
    │       └── std::array<float, 2048>   (shared wavetable data)
    │
    └── voices[8]              (polyphonic voice pool)
            │
            ▼
        AndroidVoice           (per-voice processing unit)
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

### 1. `buildAndroidWavetable()` — Wavetable Factory
**File:** `WavetableOscillator.h`

**Purpose:**
Constructs a single-cycle waveform stored as a 2048-sample float array. This waveform is the tonal raw material for every voice. It is built once at plugin construction and shared as a read-only reference across all voices.

**How it works:**
The wavetable is built by summing 10 harmonics (integer multiples of a base frequency) with amplitude values shaped to mimic the formant peaks of a sung vowel. In human voice, formants are resonant frequency bands (F1 around 300-800Hz, F2 around 1000-2500Hz) that give vowels their character. The harmonic amplitudes are deliberately boosted at harmonics 2-3 (F1 region) and 6-7 (F2 region) to bake this vowel character into the raw waveform.

**Harmonic map:**
```
Harmonic 1  → 0.50  (fundamental, moderate)
Harmonic 2  → 0.90  (F1 approach)
Harmonic 3  → 1.00  (F1 peak — loudest harmonic)
Harmonic 4  → 0.85  (F2 approach)
Harmonic 5  → 0.40  (inter-formant dip)
Harmonic 6  → 0.75  (F2 region)
Harmonic 7  → 0.80  (F2 peak)
Harmonic 8  → 0.60  (upper harmonic body)
Harmonics 9-10 → 0.25, 0.15  (air/presence, rolled off)
```

After summation the table is normalized so the peak sample equals 1.0, preventing clipping downstream.

**Key constant:**
`WAVETABLE_SIZE = 2048` — power of 2 enables efficient modulo wrapping during playback.

---

### 2. `WavetableOscillator` — Sample Generator
**File:** `WavetableOscillator.h`

**Purpose:**
Reads through the wavetable at a rate determined by the target pitch, producing one audio sample per call to `getNextSample()`. The android character comes from an LFO that continuously offsets the read position within the table, making the waveform morph rather than repeat identically each cycle.

**Core members:**
| Member | Role |
|---|---|
| `currentIndex` | Current read position in the wavetable (float for sub-sample precision) |
| `phaseIncrement` | How many table samples to advance per audio sample — set by pitch |
| `lfoPhase` | Current LFO phase (0.0 to 1.0) |
| `lfoPhaseIncrement` | LFO advance rate per audio sample |
| `lfoRate` | LFO frequency in Hz (randomized 0.3–4.5 Hz per note) |
| `lfoDepth` | Maximum table offset in samples (randomized 5–35% of table size) |

**Pitch calculation:**
```
phaseIncrement = WAVETABLE_SIZE × frequency / sampleRate
```
This maps the desired frequency to a table traversal speed. At 440Hz and 44100Hz sample rate, the oscillator advances ~20.4 table samples per audio sample.

**LFO modulation:**
Each audio sample, the LFO produces a sine value (-1.0 to +1.0) which is multiplied by `lfoDepth` to get a table offset. This offset is added to `currentIndex` before the table lookup, sweeping the read position forward and backward through the waveform. The result is continuous subtle timbral movement — the waveform never repeats exactly, producing the android restlessness.

**Linear interpolation:**
Rather than truncating `currentIndex` to an integer (which produces aliasing), the oscillator reads two adjacent samples and blends between them proportionally. This is standard wavetable practice for clean playback at all pitches.

**Randomization:**
`randomizeLFO()` is called on every `noteOn`, pulling new random values for `lfoRate` and `lfoDepth` from a Mersenne Twister RNG. This means every voice has a different modulation character — no two notes sound identical even at the same pitch.

---

### 3. `AndroidVoice` — Polyphonic Voice Unit
**File:** `AndroidVoice.h`

**Purpose:**
Wraps one oscillator, one filter, and one envelope into a single self-contained voice. The processor maintains a pool of 8 of these. Each voice is independently assigned to a MIDI note, processes its own audio, and signals when it has finished releasing.

**Signal chain per voice:**
```
WavetableOscillator → StateVariableTPTFilter → ADSR Envelope → output
```

**Filter:**
Uses JUCE's `StateVariableTPTFilter` in lowpass mode. Resonance is fixed at 0.7 — high enough that the filter itself has tonal character (sings slightly at the cutoff frequency) without self-oscillating. This adds the formant-like quality on top of what's already in the wavetable.

The cutoff frequency is not fixed — it drifts. Every 512 samples a new random target cutoff is chosen within ±200Hz of the current value, clamped between 400Hz and 5000Hz. The actual cutoff smoothly interpolates toward the target at a coefficient of 0.001 per sample, producing a slow breathing movement. This is what makes held notes feel alive rather than static.

**Envelope (ADSR):**
```
Attack  = 15ms   (fast but not clicking)
Decay   = 200ms
Sustain = 70%
Release = 400ms
```
Short attack for immediate response. Moderate release so notes don't cut off abruptly. The envelope output multiplies the filtered oscillator sample before hitting the output buffer.

**Voice lifecycle:**
1. `noteOn(midiNote, velocity)` — sets pitch, resets oscillator, randomizes LFO, triggers envelope, sets initial filter cutoff
2. `process(buffer, numSamples)` — fills output buffer, returns `false` when envelope has fully released
3. `noteOff()` — triggers envelope release phase, voice continues processing until release completes
4. Voice marks itself inactive when envelope finishes, returning it to the free pool

---

### 4. `AndroidVoiceProcessor` — Plugin Entry Point
**File:** `PluginProcessor.h` / `PluginProcessor.cpp`

**Purpose:**
The JUCE `AudioProcessor` subclass — this is the object the DAW instantiates. It owns the wavetable array and the voice pool, handles all MIDI routing, mixes voice outputs, and delivers the final audio buffer to the host.

**Initialization:**
At construction, `buildAndroidWavetable()` runs once and the result is stored in `wavetable`. Eight `AndroidVoice` objects are heap-allocated and stored as `unique_ptr` in the voices vector, each receiving a const reference to the shared wavetable.

**`prepareToPlay(sampleRate, blockSize)`:**
Called by the DAW when the audio engine starts or settings change. Propagates sample rate and block size to all voices so their internal filters and envelopes configure correctly.

**`processBlock(buffer, midiMessages)`:**
The main audio callback — called by the DAW every audio block (typically 64–512 samples).

Execution order:
1. Clear the output buffer
2. Allocate a mono mix buffer the size of the current block
3. Parse all MIDI events in the block:
   - `noteOn` → find a free voice, call `voice->noteOn()`
   - `noteOff` → find the matching voice by note number, call `voice->noteOff()`
   - `allNotesOff` → release all active voices
4. Call `process()` on every active voice, accumulating into the mono buffer
5. Copy the mono mix to all output channels (stereo spread)

**Voice allocation:**
`getFreeVoice()` iterates the pool and returns the first inactive voice. If all 8 voices are active (full polyphony), it returns `voices[0]` — the oldest voice is stolen. This is the simplest possible voice stealing strategy.

`getVoiceForNote(int note)` searches active voices for one matching a specific MIDI note number, used for note off routing.

**Output gain:**
Each voice sample is scaled by `0.3f` before accumulation. With 8 voices active simultaneously this keeps the summed output below clipping at approximately 2.4× peak, with headroom for the envelope and velocity scaling.

---

## Data Flow Summary

```
1. Plugin loads → wavetable built once (2048 floats, formant-shaped harmonics)

2. MIDI noteOn received →
   - Free voice located in pool
   - Pitch set (phaseIncrement calculated from MIDI note → Hz)
   - LFO randomized (new rate + depth for this note)
   - Filter cutoff randomized (800–3200Hz starting point)
   - ADSR triggered

3. Per audio sample (inside processBlock) →
   - LFO advances → generates offset value
   - Wavetable read at (currentIndex + lfoOffset) with interpolation
   - Sample passes through resonant lowpass (cutoff drifting slowly)
   - Multiplied by ADSR envelope value and velocity
   - Accumulated into mono mix buffer

4. MIDI noteOff received →
   - ADSR enters release phase
   - Voice continues processing until envelope reaches zero
   - Voice marks itself inactive → returns to free pool

5. Mono buffer copied to stereo output → DAW receives audio
```

---

## Known Issues (prototype_v1)

- **Hung notes** — note off messages can be missed if a note on/off pair arrives in the same block or voice stealing interrupts the note off routing. Fix in v2: track active note numbers explicitly and add a safety all-notes-off on transport stop.
- **No GUI** — all parameters are hardcoded. LFO rate/depth, filter resonance, ADSR, and output gain require a recompile to change.
- **No parameter automation** — no `AudioProcessorValueTreeState` implemented yet. DAW cannot automate any values.
- **Mono output** — both stereo channels receive identical signal. No stereo width.
- **Voice stealing is naive** — always steals voice[0] regardless of which voice is oldest or quietest.

---

## Planned v2 Features

- Fix hung note voice tracking
- `AudioProcessorValueTreeState` — expose LFO rate, LFO depth, filter cutoff, resonance, ADSR as automatable parameters
- Basic GUI — sliders for the above parameters, oscilloscope showing live wavetable output
- Second wavetable slot (sine/saw) with morphing between tables via knob
- True stereo — pan voices slightly across the field for width
- Wavefolder/soft clip distortion stage for more android grit

---

*Built: May 2026 | JUCE 7.0.9 | C++17 | VST3*
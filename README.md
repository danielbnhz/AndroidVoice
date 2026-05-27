# AndroidVoice — nth_tech wavetable synth

A JUCE VST3/Standalone wavetable synthesizer with android vocal character.

## What it does

- Wavetable built from a harmonic series shaped around vocal formant peaks (F1/F2)
- Per-voice LFO with **randomized rate and depth** sweeps the table position on every note
- Resonant lowpass filter slowly drifts cutoff per voice — breathing android quality
- 8-voice polyphony with voice stealing
- No GUI (yet) — pure sound engine

## Build (Windows, VS2022)

```bash
git clone <this repo>
cd AndroidVoice
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

First build downloads JUCE automatically (~few minutes).

Output VST3 will be in `build/AndroidVoice_artefacts/Release/VST3/`

## Build (Linux)

```bash
sudo apt install libasound2-dev libfreetype6-dev libx11-dev libxcomposite-dev \
                 libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev \
                 libxrender-dev libwebkit2gtk-4.0-dev libglu1-mesa-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j4
```

## Next steps (when ready)

- Add GUI with table position visualizer
- Expose LFO rate/depth as automatable parameters  
- Second wavetable slot with morphing between tables
- Add distortion/wavefolder for more android grit
- Formant filter (vowel ladder) as second filter stage

## Architecture

```
MIDI → AndroidVoiceProcessor
         ↓
    AndroidVoice (×8 polyphonic voices)
         ↓
    WavetableOscillator  ←  LFO (randomized per note)
         ↓
    StateVariableTPTFilter (resonant lowpass, drifting cutoff)
         ↓
    ADSR Envelope
         ↓
    Audio Output
```

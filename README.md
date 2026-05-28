# AndroidVoice - nth_tech wavetable synth

A JUCE VST3/Standalone wavetable synthesizer with android vocal character.

## What it does

- Wavetable built from a harmonic series shaped around vocal formant peaks (F1/F2)
- Per-voice LFO with **randomized rate and depth** sweeps the table position on every note
- Resonant lowpass filter slowly drifts cutoff per voice -- breathing android quality
- 8-voice polyphony with smart voice stealing
- Sample-accurate MIDI event processing -- no same-block collision artifacts
- Three-state voice lifecycle (Inactive / Playing / Releasing) -- no hung notes
- No GUI (yet) -- pure sound engine

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

## Installing in FL Studio

1. Build the VST3 (see above)
2. Close FL Studio
3. Copy the `.vst3` to `C:\Program Files\Common Files\VST3\`
4. Open FL Studio -> Options -> Manage Plugins -> Rescan
5. Remove any existing AndroidVoice instance from your project and re-add fresh

## Architecture

```
MIDI -> AndroidVoiceProcessor (sample-accurate block splitting)
         |
    AndroidVoice x8 (state machine: Inactive / Playing / Releasing)
         |
    WavetableOscillator  <--  LFO (randomized per note)
         |
    StateVariableTPTFilter (resonant lowpass, drifting cutoff)
         |
    ADSR Envelope
         |
    Audio Output
```

## Voice lifecycle (v1.1)

```
Inactive --[noteOn]--> Playing --[noteOff]--> Releasing --[envelope done]--> Inactive
                                              [forceOff]-------------------> Inactive
```

Releasing voices are invisible to note routing -- they finish their tail
cleanly without interfering with new notes on the same pitch.

## Known issues

- No GUI -- parameters are hardcoded, recompile to change values
- No parameter automation (no AudioProcessorValueTreeState yet)
- Mono output -- both stereo channels receive identical signal
- Voice stealing steals voices[0] when all 8 active; no age/amplitude awareness

## Planned v2

- GUI -- dark panel, blue/orange color scheme, live oscilloscope showing wavetable morph
- AudioProcessorValueTreeState -- LFO rate/depth, filter cutoff, resonance, ADSR as automatable parameters
- Second wavetable slot with morphing between tables via knob
- True stereo -- slight voice panning for width
- Wavefolder/soft clip distortion for more android grit
- Smarter voice stealing

---

*nth_tech | JUCE 7.0.9 | C++17 | VST3 | v1.1*

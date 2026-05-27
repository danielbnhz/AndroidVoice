#pragma once
#include "WavetableOscillator.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
//==============================================================================
// A single polyphonic voice — oscillator + resonant filter + ADSR envelope
struct AndroidVoice
{
    explicit AndroidVoice(const std::array<float, WAVETABLE_SIZE>& wavetable)
        : osc(wavetable)
    {}

    void prepare(double sr, int blockSize)
    {
        sampleRate = sr;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sr;
        spec.maximumBlockSize = (juce::uint32)blockSize;
        spec.numChannels      = 1;

        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        updateFilter();

        envelope.setSampleRate(sr);
        juce::ADSR::Parameters p;
        p.attack  = 0.015f;
        p.decay   = 0.2f;
        p.sustain = 0.7f;
        p.release = 0.4f;
        envelope.setParameters(p);
    }

    void noteOn(int midiNote, float velocity)
    {
        note = midiNote;
        this->velocity = velocity;
        float freq = 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
        osc.setFrequency(freq, sampleRate);
        osc.reset();
        osc.setActive(true);
        envelope.noteOn();

        // Randomize LFO on each new note for organic variation
        osc.randomizeLFO();

        // Randomize filter cutoff slightly per note — android voice breathes
        std::uniform_real_distribution<float> cutoffDist(800.0f, 3200.0f);
        targetCutoff  = cutoffDist(rng);
        currentCutoff = targetCutoff;
        updateFilter();
    }

    void noteOff()
    {
        envelope.noteOff();
    }

    // Returns true while voice is still sounding
    bool process(float* outputBuffer, int numSamples)
    {
        if (!osc.isActive()) return false;

        // Slowly drift filter cutoff for that breathing android quality
        filterDriftCounter++;
        if (filterDriftCounter > 512)
        {
            filterDriftCounter = 0;
            std::uniform_real_distribution<float> drift(-200.0f, 200.0f);
            targetCutoff = juce::jlimit(400.0f, 5000.0f, targetCutoff + drift(rng));
        }
        currentCutoff += (targetCutoff - currentCutoff) * 0.001f; // smooth glide
        filter.setCutoffFrequency(currentCutoff);

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = osc.getNextSample();
            sample = filter.processSample(0, sample);
            float env = envelope.getNextSample();
            outputBuffer[i] += sample * env * velocity * 0.3f;

            if (!envelope.isActive())
            {
                osc.setActive(false);
                return false;
            }
        }
        return true;
    }

    int  note     = -1;
    bool isActive() const { return osc.isActive(); }

private:
    void updateFilter()
    {
        filter.setCutoffFrequency(currentCutoff);
        filter.setResonance(0.7f); // Keep resonance up — formant character
    }

    WavetableOscillator osc;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::ADSR envelope;

    double sampleRate    = 44100.0;
    float  velocity      = 1.0f;
    float  currentCutoff = 1200.0f;
    float  targetCutoff  = 1200.0f;
    int    filterDriftCounter = 0;

    std::mt19937 rng{ std::random_device{}() };
};

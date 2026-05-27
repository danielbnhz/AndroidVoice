#pragma once
#include <JuceHeader.h>
#include <array>
#include <random>

// Wavetable size — power of 2 for fast wrapping
static constexpr int WAVETABLE_SIZE = 2048;

// Builds a vocal-formant inspired wavetable by summing harmonics
// with formant-shaped amplitude envelope — gives that android vowel body
static std::array<float, WAVETABLE_SIZE> buildAndroidWavetable()
{
    std::array<float, WAVETABLE_SIZE> table{};

    // Harmonic series with formant peaks around harmonics 2-4 and 6-8
    // mimics the F1/F2 formant structure of a sung vowel
    struct Harmonic { int number; float amplitude; };
    std::array<Harmonic, 10> harmonics = {{
        {1,  0.5f},
        {2,  0.9f},   // F1 region — boosted
        {3,  1.0f},   // F1 peak
        {4,  0.85f},  // F2 approach
        {5,  0.4f},
        {6,  0.75f},  // F2 region
        {7,  0.8f},   // F2 peak
        {8,  0.6f},
        {9,  0.25f},
        {10, 0.15f},
    }};

    for (int i = 0; i < WAVETABLE_SIZE; ++i)
    {
        float sample = 0.0f;
        float phase  = juce::MathConstants<float>::twoPi * (float)i / (float)WAVETABLE_SIZE;

        for (auto& h : harmonics)
            sample += h.amplitude * std::sin((float)h.number * phase);

        table[i] = sample;
    }

    // Normalize
    float peak = *std::max_element(table.begin(), table.end(),
                    [](float a, float b){ return std::abs(a) < std::abs(b); });
    if (peak > 0.0f)
        for (auto& s : table) s /= peak;

    return table;
}

//==============================================================================
class WavetableOscillator
{
public:
    WavetableOscillator(const std::array<float, WAVETABLE_SIZE>& wt)
        : wavetable(wt), rng(std::random_device{}())
    {
        randomizeLFO();
    }

    void setFrequency(float freqHz, double sampleRate)
    {
        phaseIncrement = (float)WAVETABLE_SIZE * freqHz / (float)sampleRate;
        lfoPhaseIncrement = lfoRate / (float)sampleRate;
    }

    void setSampleRate(double sr) { sampleRate = sr; }

    // Randomize the LFO modulating table position — this is the android "wobble"
    void randomizeLFO()
    {
        std::uniform_real_distribution<float> rateDist(0.3f, 4.5f);   // Hz
        std::uniform_real_distribution<float> depthDist(0.05f, 0.35f); // fraction of table
        lfoRate  = rateDist(rng);
        lfoDepth = depthDist(rng) * (float)WAVETABLE_SIZE;
    }

    float getNextSample()
    {
        // Advance LFO
        lfoPhase += lfoPhaseIncrement;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

        float lfoValue = std::sin(lfoPhase * juce::MathConstants<float>::twoPi);

        // Table position offset driven by LFO — sweeps the wavetable
        float modulatedIndex = currentIndex + lfoValue * lfoDepth;

        // Wrap into table bounds
        while (modulatedIndex >= (float)WAVETABLE_SIZE) modulatedIndex -= (float)WAVETABLE_SIZE;
        while (modulatedIndex <  0.0f)                  modulatedIndex += (float)WAVETABLE_SIZE;

        // Linear interpolation between samples
        int   indexA  = (int)modulatedIndex;
        int   indexB  = (indexA + 1) % WAVETABLE_SIZE;
        float frac    = modulatedIndex - (float)indexA;
        float sample  = wavetable[indexA] + frac * (wavetable[indexB] - wavetable[indexA]);

        // Advance main phase
        currentIndex += phaseIncrement;
        if (currentIndex >= (float)WAVETABLE_SIZE)
            currentIndex -= (float)WAVETABLE_SIZE;

        return sample;
    }

    bool isActive() const { return active; }
    void setActive(bool a) { active = a; }
    void reset() { currentIndex = 0.0f; lfoPhase = 0.0f; randomizeLFO(); }

private:
    const std::array<float, WAVETABLE_SIZE>& wavetable;

    float currentIndex     = 0.0f;
    float phaseIncrement   = 0.0f;

    // LFO for table position modulation
    float lfoPhase          = 0.0f;
    float lfoPhaseIncrement = 0.0f;
    float lfoRate           = 1.0f;
    float lfoDepth          = 100.0f;

    double sampleRate = 44100.0;
    bool   active     = false;

    std::mt19937 rng;
};

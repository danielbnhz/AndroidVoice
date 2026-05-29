#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "AndroidVoice.h"

static constexpr int MAX_VOICES = 8;

class AndroidVoiceProcessor : public juce::AudioProcessor
{
public:
    AndroidVoiceProcessor();
    ~AndroidVoiceProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    const juce::String getName() const override { return "AndroidVoice"; }
    bool   acceptsMidi()  const override { return true; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
    }

private:
    std::array<float, WAVETABLE_SIZE> wavetable;
    std::vector<std::unique_ptr<AndroidVoice>> voices;
    juce::AudioProcessorValueTreeState apvts;
    
    AndroidVoice* getFreeVoice();
    AndroidVoice* getVoiceForNote(int note);
    void renderVoices(float* buffer, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AndroidVoiceProcessor)
};

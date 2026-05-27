#include "PluginProcessor.h"

AndroidVoiceProcessor::AndroidVoiceProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    wavetable = buildAndroidWavetable();

    for (int i = 0; i < MAX_VOICES; ++i)
        voices.push_back(std::make_unique<AndroidVoice>(wavetable));
}

void AndroidVoiceProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    for (auto& v : voices)
        v->prepare(sampleRate, samplesPerBlock);
}

void AndroidVoiceProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Mono processing buffer then copy to all output channels
    std::vector<float> monoBuffer(buffer.getNumSamples(), 0.0f);

    // Handle MIDI events
    for (const auto meta : midiMessages)
    {
        auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            auto* voice = getFreeVoice();
            if (voice)
                voice->noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
        }
        else if (msg.isNoteOff())
        {
            auto* voice = getVoiceForNote(msg.getNoteNumber());
            if (voice)
                voice->noteOff();
        }
        else if (msg.isAllNotesOff())
        {
            for (auto& v : voices)
                v->noteOff();
        }
    }

    // Process all active voices into mono buffer
    for (auto& v : voices)
    {
        if (v->isActive())
            v->process(monoBuffer.data(), buffer.getNumSamples());
    }

    // Copy mono mix to all output channels
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* channelData = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            channelData[i] += monoBuffer[i];
    }
}

AndroidVoice* AndroidVoiceProcessor::getFreeVoice()
{
    for (auto& v : voices)
        if (!v->isActive())
            return v.get();
    // Voice steal — return first voice (oldest)
    return voices[0].get();
}

AndroidVoice* AndroidVoiceProcessor::getVoiceForNote(int note)
{
    for (auto& v : voices)
        if (v->isActive() && v->note == note)
            return v.get();
    return nullptr;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AndroidVoiceProcessor();
}

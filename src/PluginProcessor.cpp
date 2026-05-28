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

    std::vector<float> monoBuffer(buffer.getNumSamples(), 0.0f);

    int blockStart = 0;

    for (const auto meta : midiMessages)
    {
        const int eventPos = meta.samplePosition;
        auto msg = meta.getMessage();

        if (eventPos > blockStart)
        {
            renderVoices(monoBuffer.data() + blockStart, eventPos - blockStart);
            blockStart = eventPos;
        }

        if (msg.isNoteOn() && msg.getVelocity() > 0)
        {
            const int noteNum = msg.getNoteNumber();

            // Release any voice currently playing this exact note
            if (auto* existing = getVoiceForNote(noteNum))
                existing->noteOff();

            auto* voice = getFreeVoice();
            if (voice)
                voice->noteOn(noteNum, msg.getFloatVelocity());
        }
        else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
        {
            if (auto* voice = getVoiceForNote(msg.getNoteNumber()))
                voice->noteOff();
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (auto& v : voices)
                v->forceOff();
        }
    }

    if (blockStart < buffer.getNumSamples())
        renderVoices(monoBuffer.data() + blockStart, buffer.getNumSamples() - blockStart);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* channelData = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            channelData[i] += monoBuffer[i];
    }
}

void AndroidVoiceProcessor::renderVoices(float* buffer, int numSamples)
{
    if (numSamples <= 0) return;
    for (auto& v : voices)
        if (v->isActive())
            v->process(buffer, numSamples);
}

AndroidVoice* AndroidVoiceProcessor::getFreeVoice()
{
    // Prefer truly inactive voices first
    for (auto& v : voices)
        if (!v->isActive())
            return v.get();

    // Then steal a releasing voice (already fading out)
    for (auto& v : voices)
        if (!v->isPlayingNote(v->note))
            return v.get();

    // Last resort: steal oldest voice
    voices[0]->forceOff();
    return voices[0].get();
}

AndroidVoice* AndroidVoiceProcessor::getVoiceForNote(int note)
{
    // Only match voices actively PLAYING this note
    // Releasing voices are intentionally excluded
    for (auto& v : voices)
        if (v->isPlayingNote(note))
            return v.get();
    return nullptr;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AndroidVoiceProcessor();
}

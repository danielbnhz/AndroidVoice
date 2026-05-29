#include "PluginProcessor.h"

AndroidVoiceProcessor::AndroidVoiceProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
        apvts(*this, nullptr, "Parameters", createParameterLayout())
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


    float attack  = apvts.getRawParameterValue("attack")->load();
    float decay   = apvts.getRawParameterValue("decay")->load();
    float sustain = apvts.getRawParameterValue("sustain")->load();
    float release = apvts.getRawParameterValue("release")->load();

    juce::ADSR::Parameters adsrParams{ attack, decay, sustain, release };
    for (auto& v : voices)
        v->setADSRParameters(adsrParams);


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


juce::AudioProcessorValueTreeState::ParameterLayout 
AndroidVoiceProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lfo_rate", "LFO Rate", 0.3f, 4.5f, 1.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lfo_depth", "LFO Depth", 0.05f, 0.35f, 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filter_cutoff", "Filter Cutoff", 400.f, 5000.f, 2000.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "attack", "Attack", 0.001f, 0.5f, 0.015f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "decay", "Decay", 0.01f, 1.0f, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "sustain", "Sustain", 0.0f, 1.0f, 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "release", "Release", 0.05f, 2.0f, 0.4f));

    return { params.begin(), params.end() };
}
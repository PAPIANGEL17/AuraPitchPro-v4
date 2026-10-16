#include "PluginProcessor.h"
#include "PluginEditor.h"

AuraPitchAudioProcessor::AuraPitchAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    formatManager.registerBasicFormats();
    for (auto& pt : visualHistory)
    {
        pt.originalMidi = 62.0f;
        pt.correctedMidi = 62.0f;
        pt.targetNoteMidi = 62.0f;
        pt.amplitude = 0.0f;
        pt.voiced = false;
    }
}

AuraPitchAudioProcessor::~AuraPitchAudioProcessor()
{
    transportSource.setSource(nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout AuraPitchAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "CENTER_SNAP", "Pitch Center", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.90f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DRIFT_CORR", "Pitch Drift", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.85f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "VIBRATO_PRESERVE", "Natural Expression", juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TRANSITION_SMOOTH", "Note Transition", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "FORMANT_SHIFT", "Throat Formant", juce::NormalisableRange<float>(-6.0f, 6.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "MIX_LEVEL", "Mix Level", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ONSET_SHAPING", "AI Attack Shape", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TUBE_WARMTH", "Analog Tube Warmth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    juce::StringArray roots = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>("ROOT_NOTE", "Root Key", roots, 2));

    juce::StringArray scales = { "Chromatic (Auto-Free)", "Major", "Minor (Urban/Natural)" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>("SCALE_TYPE", "Scale Mode", scales, 2));

    return { params.begin(), params.end() };
}

bool AuraPitchAudioProcessor::loadAudioFile(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr)
    {
        transportSource.stop();
        transportSource.setSource(nullptr);
        readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        readerSource->setLooping(isLoopingState);
        transportSource.setSource(readerSource.get(), 0, nullptr, reader->sampleRate);
        currentFileName = file.getFileName();
        fileLoaded = true;
        transportSource.start();
        return true;
    }
    return false;
}

void AuraPitchAudioProcessor::togglePlayback()
{
    if (!fileLoaded) return;
    if (transportSource.isPlaying())
        transportSource.stop();
    else
    {
        if (transportSource.hasStreamFinished())
            transportSource.setPosition(0.0);
        transportSource.start();
    }
}

void AuraPitchAudioProcessor::stopPlayback()
{
    transportSource.stop();
    transportSource.setPosition(0.0);
}

void AuraPitchAudioProcessor::setLooping(bool shouldLoop)
{
    isLoopingState = shouldLoop;
    if (readerSource != nullptr)
        readerSource->setLooping(shouldLoop);
}

void AuraPitchAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    pitchEngine.prepare(sampleRate, samplesPerBlock);
    transportSource.prepareToPlay(samplesPerBlock, sampleRate);
    setLatencySamples(0); // Latencia Cero Real
}

void AuraPitchAudioProcessor::releaseResources()
{
    transportSource.releaseResources();
}

bool AuraPitchAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void AuraPitchAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    if (fileLoaded && transportSource.isPlaying())
    {
        if (readerSource != nullptr)
            readerSource->setLooping(isLoopingState);

        juce::AudioSourceChannelInfo info(&buffer, 0, numSamples);
        transportSource.getNextAudioBlock(info);
    }

    // Leer valores actuales de los perillas del APVTS
    float centerSnap = apvts.getRawParameterValue("CENTER_SNAP")->load();
    float driftCorr  = apvts.getRawParameterValue("DRIFT_CORR")->load();
    float vibrato    = apvts.getRawParameterValue("VIBRATO_PRESERVE")->load();
    float transition = apvts.getRawParameterValue("TRANSITION_SMOOTH")->load();
    float formant    = apvts.getRawParameterValue("FORMANT_SHIFT")->load();
    float mixLevel   = apvts.getRawParameterValue("MIX_LEVEL")->load();
    float onsetShape = apvts.getRawParameterValue("ONSET_SHAPING")->load();
    float tubeWarm   = apvts.getRawParameterValue("TUBE_WARMTH")->load();
    int rootNote     = static_cast<int>(apvts.getRawParameterValue("ROOT_NOTE")->load());
    int scaleType    = static_cast<int>(apvts.getRawParameterValue("SCALE_TYPE")->load());

    // Procesar canal izquierdo (Mono o Estéreo L)
    auto* channelData = buffer.getWritePointer(0);
    float rms = buffer.getRMSLevel(0, 0, numSamples);
    inputLevelDb.store(juce::Decibels::gainToDecibels(rms, -60.0f));

    // LLAMADA ACTIVA AL MOTOR DSP
    pitchEngine.processBlock(channelData, numSamples,
                             driftCorr, centerSnap, vibrato, transition, formant, mixLevel,
                             onsetShape, tubeWarm, rootNote, scaleType);

    // Duplicar canal L hacia R si es estéreo para procesar ambos lados idénticos
    if (totalNumOutputChannels > 1)
    {
        auto* rightChannel = buffer.getWritePointer(1);
        juce::FloatVectorOperations::copy(rightChannel, channelData, numSamples);
    }

    currentCents.store(pitchEngine.getCentsDeviation());
    if (pitchEngine.isCurrentlyVoiced())
        currentMidiNote.store(pitchEngine.getPureScaleNoteMidi());

    isSibilantBypass.store(!pitchEngine.isCurrentlyVoiced() && rms > 0.004f);

    uiPushSubCounter += numSamples;
    if (uiPushSubCounter >= 512)
    {
        uiPushSubCounter = 0;
        BlobHistoryPoint pt;
        pt.originalMidi   = pitchEngine.getLastDetectedMidi();
        pt.correctedMidi  = pitchEngine.getLastTargetMidi();
        pt.targetNoteMidi = pitchEngine.getPureScaleNoteMidi();
        pt.amplitude      = juce::jlimit(0.0f, 1.0f, rms * 5.5f);
        pt.voiced         = pitchEngine.isCurrentlyVoiced();

        int idx = historyWriteIndex.load();
        visualHistory[idx] = pt;
        historyWriteIndex.store((idx + 1) % static_cast<int>(visualHistory.size()));
    }
}

void AuraPitchAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AuraPitchAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* AuraPitchAudioProcessor::createEditor()
{
    return new AuraPitchAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AuraPitchAudioProcessor();
}

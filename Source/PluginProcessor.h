#pragma once
#include <JuceHeader.h>
#include "NaturalPitchEngine.h"
#include <array>

struct BlobHistoryPoint
{
    float originalMidi = 62.0f;
    float correctedMidi = 62.0f;
    float targetNoteMidi = 62.0f;
    float amplitude = 0.0f;
    bool voiced = false;
};

class AuraPitchAudioProcessor : public juce::AudioProcessor
{
public:
    AuraPitchAudioProcessor();
    ~AuraPitchAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool loadAudioFile(const juce::File& file);
    void togglePlayback();
    void stopPlayback();
    void setLooping(bool shouldLoop);
    bool isPlayingAudio() const { return transportSource.isPlaying(); }
    bool isLoopingAudio() const { return isLoopingState; }
    bool hasLoadedFile() const { return fileLoaded; }
    juce::String getLoadedFileName() const { return currentFileName; }

    juce::AudioProcessorValueTreeState apvts;
    std::array<BlobHistoryPoint, 220> visualHistory;
    std::atomic<int> historyWriteIndex { 0 };
    std::atomic<float> currentCents { 0.0f };
    std::atomic<float> currentMidiNote { 62.0f };
    std::atomic<bool> isSibilantBypass { false };
    std::atomic<float> inputLevelDb { -60.0f };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    NaturalPitchEngine pitchEngine;

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    juce::String currentFileName { "No Audio File Loaded" };
    bool fileLoaded = false;
    bool isLoopingState = true;
    int uiPushSubCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraPitchAudioProcessor)
};

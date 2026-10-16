#include "PluginProcessor.h"
#include "PluginEditor.h"

AuraPitchAudioProcessorEditor::AuraPitchAudioProcessorEditor(AuraPitchAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&cyberLnf);
    setSize(920, 580);

    setupKnob(centerKnob,     centerLabel,     "PITCH CENTER", " %");
    setupKnob(driftKnob,      driftLabel,      "PITCH DRIFT",  " %");
    setupKnob(vibratoKnob,    vibratoLabel,    "NATURAL EXPR", " x");
    setupKnob(transitionKnob, transitionLabel, "TRANSITION",   " s");
    setupKnob(formantKnob,    formantLabel,    "FORMANT SHIFT"," st");

    addAndMakeVisible(rootCombo);
    rootCombo.addItemList({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);

    addAndMakeVisible(scaleCombo);
    scaleCombo.addItemList({ "Chromatic (Auto-Free)", "Major", "Minor (Urban/Natural)" }, 1);

    centerAtt     = std::make_unique<SliderAttachment>(audioProcessor.apvts, "CENTER_SNAP", centerKnob);
    driftAtt      = std::make_unique<SliderAttachment>(audioProcessor.apvts, "DRIFT_CORR", driftKnob);
    vibratoAtt    = std::make_unique<SliderAttachment>(audioProcessor.apvts, "VIBRATO_PRESERVE", vibratoKnob);
    transitionAtt = std::make_unique<SliderAttachment>(audioProcessor.apvts, "TRANSITION_SMOOTH", transitionKnob);
    formantAtt    = std::make_unique<SliderAttachment>(audioProcessor.apvts, "FORMANT_SHIFT", formantKnob);
    rootAtt       = std::make_unique<ComboAttachment>(audioProcessor.apvts, "ROOT_NOTE", rootCombo);
    scaleAtt      = std::make_unique<ComboAttachment>(audioProcessor.apvts, "SCALE_TYPE", scaleCombo);

    addAndMakeVisible(loadBtn);
    loadBtn.onClick = [this]() { openAudioFileChooser(); };

    addAndMakeVisible(playBtn);
    playBtn.onClick = [this]() { audioProcessor.togglePlayback(); };

    addAndMakeVisible(stopBtn);
    stopBtn.onClick = [this]() { audioProcessor.stopPlayback(); };

    addAndMakeVisible(loopBtn);
    loopBtn.setClickingTogglesState(true);
    loopBtn.setToggleState(true, juce::dontSendNotification);
    loopBtn.onClick = [this]()
    {
        bool state = loopBtn.getToggleState();
        audioProcessor.setLooping(state);
        loopBtn.setButtonText(state ? "LOOP: ON" : "LOOP: OFF");
    };

    startTimerHz(60);
}

AuraPitchAudioProcessorEditor::~AuraPitchAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void AuraPitchAudioProcessorEditor::setupKnob(juce::Slider& slider, juce::Label& label,
                                              const juce::String& text, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 20);
    slider.setTextValueSuffix(suffix);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff00e5ff));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff04060a));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff1e3250));
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour(0xff96b4d2));
    addAndMakeVisible(label);
}

void AuraPitchAudioProcessorEditor::openAudioFileChooser()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select a Vocal Audio File (WAV, MP3, AIFF, FLAC)...",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.wav;*.mp3;*.aiff;*.flac;*.ogg");

    auto folderChooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();
        if (file.existsAsFile())
        {
            audioProcessor.loadAudioFile(file);
            repaint();
        }
    });
}

bool AuraPitchAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase(".wav") || f.endsWithIgnoreCase(".mp3") ||
            f.endsWithIgnoreCase(".flac") || f.endsWithIgnoreCase(".aiff"))
            return true;
    return false;
}

void AuraPitchAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    if (files.size() > 0)
        audioProcessor.loadAudioFile(juce::File(files[0]));
}

juce::String AuraPitchAudioProcessorEditor::midiToNoteName(int midiNote) const
{
    const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int n = (midiNote + 1200) % 12;
    int oct = (midiNote / 12) - 1;
    return juce::String(names[n]) + juce::String(oct);
}

void AuraPitchAudioProcessorEditor::timerCallback()
{
    float targetNote = audioProcessor.currentMidiNote.load();
    displayedCenterMidi = 0.92f * displayedCenterMidi + 0.08f * targetNote;
    repaint();
}

void AuraPitchAudioProcessorEditor::paint(juce::Graphics& g)
{
    // 1. 3D TITANIUM BRUSHED METAL CHASSIS
    juce::ColourGradient chassisGrad(juce::Colour(0xff1c2436), 0.0f, 0.0f,
                                     juce::Colour(0xff080c16), 0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(chassisGrad);
    g.fillAll();

    g.setColour(juce::Colour(0xff7896be).withAlpha(0.85f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 12.0f, 2.0f);
    g.setColour(juce::Colour(0xff020408));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(5.0f), 10.0f, 3.0f);

    // 2. HEADER BRANDING & EMBLEM LOGO (ANGEL MUSIC RECORDS)
    g.setColour(juce::Colour(0xffe0f0ff));
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText("AURAPITCH PRO", 28, 16, 280, 28, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff00e5ff));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText("3D CYBER-GLASS WSOLA PITCH & FORMANT SUITE", 30, 43, 350, 14, juce::Justification::centredLeft);

    // Dibujar emblema vectorial elegante inspirado en tu logo (AMR / Angel Music Records Sun-Burst)
    juce::Rectangle<float> logoBox(392.0f, 16.0f, 36.0f, 36.0f);
    g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.25f));
    g.fillEllipse(logoBox);
    g.setColour(juce::Colour(0xff00e5ff));
    g.drawEllipse(logoBox, 1.5f);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("AMR", logoBox, juce::Justification::centred);

    // 3. 3D RECESSED STROBE TUNER BOX
    juce::Rectangle<float> tunerBox(442.0f, 15.0f, 205.0f, 44.0f);
    g.setColour(juce::Colour(0xff04060a));
    g.fillRoundedRectangle(tunerBox, 6.0f);
    g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.5f));
    g.drawRoundedRectangle(tunerBox, 6.0f, 1.2f);

    int activeMidi = static_cast<int>(std::round(audioProcessor.currentMidiNote.load()));
    g.setColour(juce::Colour(0xff00e5ff));
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText(midiToNoteName(activeMidi), static_cast<int>(tunerBox.getX() + 14), static_cast<int>(tunerBox.getY() + 6), 54, 32, juce::Justification::centred);

    float meterX = tunerBox.getX() + 62.0f;
    float meterW = 95.0f;
    float meterY = tunerBox.getCentreY();
    g.setColour(juce::Colour(0xff030508));
    g.fillRoundedRectangle(meterX, meterY - 4.0f, meterW, 8.0f, 4.0f);
    g.setColour(juce::Colour(0xff00e5ff));
    g.drawVerticalLine(static_cast<int>(meterX + meterW * 0.5f), meterY - 8.0f, meterY + 8.0f);
    g.fillEllipse(meterX + meterW * 0.5f - 5.0f, meterY - 5.0f, 10.0f, 10.0f);
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText("LOCKED", static_cast<int>(tunerBox.getRight() - 54), static_cast<int>(tunerBox.getY() + 12), 48, 20, juce::Justification::centred);

    // 4. 3D RECESSED TRANSPORT BAR
    juce::Rectangle<float> transportBar(26.0f, 68.0f, static_cast<float>(getWidth() - 52), 34.0f);
    g.setColour(juce::Colour(0xff06090f));
    g.fillRoundedRectangle(transportBar, 6.0f);
    g.setColour(juce::Colour(0xff141e30));
    g.drawRoundedRectangle(transportBar, 6.0f, 1.5f);

    g.setColour(audioProcessor.hasLoadedFile() ? juce::Colour(0xff00e5ff) : juce::Colour(0xff96b4d2));
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    juce::String fileStatus = audioProcessor.hasLoadedFile()
        ? ("LOADED: " + audioProcessor.getLoadedFileName() + (audioProcessor.isPlayingAudio() ? "  [PLAYING]" : "  [PAUSED]"))
        : "STANDALONE & DAW PLAYER: Drag & Drop WAV/MP3 here or click IMPORT AUDIO";
    g.drawText(fileStatus, 435, 71, 445, 28, juce::Justification::centredLeft);

    // ==============================================================================
    // // 5. 3D GLASS PIANO-ROLL WINDOW WITH FROSTED GLASS & HOLOGRAPHIC BLOBS
    // ==============================================================================
    juce::Rectangle<float> displayRect(26.0f, 110.0f, static_cast<float>(getWidth() - 52), 265.0f);
    
    // Deep recessed shadow frame
    g.setColour(juce::Colour(0xff020305));
    g.fillRoundedRectangle(displayRect.expanded(3.0f), 11.0f);

    // Glass panel body
    g.setColour(juce::Colour(0xff0c1422));
    g.fillRoundedRectangle(displayRect, 8.0f);

    const float pianoKeyWidth = 54.0f;
    const int numVisibleNotes = 11;
    float laneHeight = displayRect.getHeight() / static_cast<float>(numVisibleNotes);
    int centerMidiInt = static_cast<int>(std::round(displayedCenterMidi));
    int rootNote = static_cast<int>(audioProcessor.apvts.getRawParameterValue("ROOT_NOTE")->load());
    int scaleType = static_cast<int>(audioProcessor.apvts.getRawParameterValue("SCALE_TYPE")->load());

    const bool majorMask[12] = {true, false, true, false, true, true, false, true, false, true, false, true};
    const bool minorMask[12] = {true, false, true, true, false, true, false, true, true, false, true, false};

    for (int i = 0; i < numVisibleNotes; ++i)
    {
        int noteMidi = centerMidiInt + (numVisibleNotes / 2) - i;
        float laneTop = displayRect.getY() + i * laneHeight;
        int rel = (noteMidi - rootNote + 1200) % 12;
        bool inScale = (scaleType == 0) ? true : (scaleType == 1 ? majorMask[rel] : minorMask[rel]);
        int noteInOct = (noteMidi + 1200) % 12;
        bool isBlackKey = (noteInOct == 1 || noteInOct == 3 || noteInOct == 6 || noteInOct == 8 || noteInOct == 10);

        g.setColour(inScale ? juce::Colour(0xff122038) : juce::Colour(0xff080d16));
        g.fillRect(displayRect.getX() + pianoKeyWidth, laneTop, displayRect.getWidth() - pianoKeyWidth, laneHeight);

        g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.15f));
        g.drawHorizontalLine(static_cast<int>(laneTop), displayRect.getX() + pianoKeyWidth, displayRect.getRight());

        juce::Rectangle<float> keyRect(displayRect.getX(), laneTop, pianoKeyWidth, laneHeight);
        g.setColour(isBlackKey ? juce::Colour(0xff16243a) : juce::Colour(0xff233452));
        g.fillRect(keyRect.reduced(0.5f));
        g.setColour(juce::Colour(0xff648caa).withAlpha(0.6f));
        g.drawHorizontalLine(static_cast<int>(laneTop + 1), displayRect.getX(), displayRect.getX() + pianoKeyWidth);

        if (inScale)
        {
            g.setColour(juce::Colour(0xff00e5ff));
            g.fillRect(displayRect.getX() + pianoKeyWidth - 4.0f, laneTop + 1.0f, 4.0f, laneHeight - 2.0f);
        }

        g.setColour(inScale ? juce::Colour(0xfff0f5ff) : juce::Colour(0xff5a7896));
        g.setFont(juce::FontOptions(11.0f, inScale ? juce::Font::bold : juce::Font::plain));
        g.drawText(midiToNoteName(noteMidi), keyRect.reduced(6.0f, 0.0f), juce::Justification::centredLeft);
    }

    float canvasX = displayRect.getX() + pianoKeyWidth;
    float canvasW = displayRect.getWidth() - pianoKeyWidth;
    g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.18f));
    for (int gx = 1; gx < 8; ++gx)
    {
        float xPos = canvasX + (canvasW / 8.0f) * static_cast<float>(gx);
        g.drawVerticalLine(static_cast<int>(xPos), displayRect.getY(), displayRect.getBottom());
    }

    const auto& history = audioProcessor.visualHistory;
    int histSize = static_cast<int>(history.size());
    int readIdx = audioProcessor.historyWriteIndex.load();
    float stepX = canvasW / static_cast<float>(histSize);

    auto midiToY = [&](float midiVal) -> float
    {
        float topMidi = static_cast<float>(centerMidiInt + (numVisibleNotes / 2)) + 0.5f;
        float norm = (topMidi - midiVal) / static_cast<float>(numVisibleNotes);
        return juce::jlimit(displayRect.getY() + 4.0f, displayRect.getBottom() - 4.0f,
                            displayRect.getY() + norm * displayRect.getHeight());
    };

    juce::Path originalPitchPath;
    juce::Path correctedPitchPath;
    bool lineActive = false;

    for (int i = 0; i < histSize; ++i)
    {
        int idx = (readIdx + i) % histSize;
        const auto& pt = history[idx];
        float x = canvasX + static_cast<float>(i) * stepX;

        if (pt.voiced && pt.amplitude > 0.015f)
        {
            float yTargetNote = midiToY(pt.targetNoteMidi);
            float yOrig       = midiToY(pt.originalMidi);
            float yCorr       = midiToY(pt.correctedMidi);
            float halfThick   = juce::jlimit(3.0f, laneHeight * 0.45f, pt.amplitude * laneHeight * 0.44f);

            // 3D Glass Blob capsule with shadow and highlight
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRect(x, yTargetNote - halfThick + 1.0f, stepX + 1.2f, halfThick * 2.0f);

            g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.35f));
            g.fillRect(x, yTargetNote - halfThick, stepX + 1.2f, halfThick * 2.0f);

            g.setColour(juce::Colour(0xffdcfff5).withAlpha(0.95f));
            g.fillRect(x, yTargetNote - halfThick, stepX + 1.0f, 1.8f);

            if (!lineActive)
            {
                originalPitchPath.startNewSubPath(x, yOrig);
                correctedPitchPath.startNewSubPath(x, yCorr);
                lineActive = true;
            }
            else
            {
                originalPitchPath.lineTo(x, yOrig);
                correctedPitchPath.lineTo(x, yCorr);
            }
        }
        else
        {
            lineActive = false;
        }
    }

    g.setColour(juce::Colour(0xffff64c8).withAlpha(0.75f));
    g.strokePath(originalPitchPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(juce::Colour(0xff00f5ff));
    g.strokePath(correctedPitchPath, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Glass specular rim
    g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.85f));
    g.drawRoundedRectangle(displayRect, 8.0f, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.drawLine(displayRect.getX() + 2.0f, displayRect.getY() + 1.0f, displayRect.getRight() - 2.0f, displayRect.getY() + 1.0f, 1.5f);

    // 6. LOWER 3D RACK PANEL
    juce::Rectangle<float> rackPanel(26.0f, 386.0f, static_cast<float>(getWidth() - 52), 172.0f);
    juce::ColourGradient rackGrad(juce::Colour(0xff141c2c), rackPanel.getX(), rackPanel.getY(),
                                  juce::Colour(0xff080c18), rackPanel.getX(), rackPanel.getBottom(), false);
    g.setGradientFill(rackGrad);
    g.fillRoundedRectangle(rackPanel, 8.0f);
    g.setColour(juce::Colour(0xff284164));
    g.drawRoundedRectangle(rackPanel, 8.0f, 2.0f);
}

void AuraPitchAudioProcessorEditor::resized()
{
    rootCombo.setBounds(640, 22, 75, 30);
    scaleCombo.setBounds(725, 22, 169, 30);

    loadBtn.setBounds(36, 73, 118, 24);
    playBtn.setBounds(162, 73, 102, 24);
    stopBtn.setBounds(272, 73, 68, 24);
    loopBtn.setBounds(348, 73, 76, 24);

    int knobY = 396;
    int knobW = 128;
    int knobH = 128;
    int spacing = 168;
    int startX = 58;

    centerKnob.setBounds(startX, knobY, knobW, knobH);
    centerLabel.setBounds(startX, knobY + knobH + 2, knobW, 20);

    driftKnob.setBounds(startX + spacing, knobY, knobW, knobH);
    driftLabel.setBounds(startX + spacing, knobY + knobH + 2, knobW, 20);

    vibratoKnob.setBounds(startX + spacing * 2, knobY, knobW, knobH);
    vibratoLabel.setBounds(startX + spacing * 2, knobY + knobH + 2, knobW, 20);

    transitionKnob.setBounds(startX + spacing * 3, knobY, knobW, knobH);
    transitionLabel.setBounds(startX + spacing * 3, knobY + knobH + 2, knobW, 20);

    formantKnob.setBounds(startX + spacing * 4, knobY, knobW, knobH);
    formantLabel.setBounds(startX + spacing * 4, knobY + knobH + 2, knobW, 20);
}

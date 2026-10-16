#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ==============================================================================
// // 3D CYBER-GLASS LOOK AND FEEL
// ==============================================================================
class CyberGlassLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CyberGlassLookAndFeel()
    {
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0a0e18));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff00e5ff).withAlpha(0.6f));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xffe0f0ff));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff060a12));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffe0f0ff));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff00e5ff).withAlpha(0.35f));

        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff101726));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00e5ff));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00e5ff));
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xff04060a));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(10.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        const int numTicks = 25;
        for (int i = 0; i < numTicks; ++i)
        {
            float prop = static_cast<float>(i) / static_cast<float>(numTicks - 1);
            float angle = rotaryStartAngle + prop * (rotaryEndAngle - rotaryStartAngle) - juce::MathConstants<float>::halfPi;
            bool active = (prop <= sliderPos && sliderPos > 0.005f);

            float rIn  = radius - 2.0f;
            float rOut = radius + 2.8f;
            juce::Point<float> p1(centreX + rIn * std::cos(angle), centreY + rIn * std::sin(angle));
            juce::Point<float> p2(centreX + rOut * std::cos(angle), centreY + rOut * std::sin(angle));

            g.setColour(active ? juce::Colour(0xff00e5ff) : juce::Colour(0xff1a2538));
            g.drawLine(juce::Line<float>(p1, p2), active ? 2.2f : 1.2f);
        }

        float arcRadius = radius - 6.5f;
        juce::Path bgArc;
        bgArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff0a1220));
        g.strokePath(bgArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (sliderPos > 0.001f)
        {
            juce::Path valArc;
            valArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
            g.setColour(juce::Colour(0xff00e5ff));
            g.strokePath(valArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        float knobR = radius - 12.0f;
        juce::Rectangle<float> knobRect(centreX - knobR, centreY - knobR, knobR * 2.0f, knobR * 2.0f);

        // 3D Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.fillEllipse(knobRect.translated(0.0f, 4.0f));

        // 3D Bevel Rim
        juce::ColourGradient knobGrad(juce::Colour(0xff324b6e), centreX - knobR * 0.5f, centreY - knobR,
                                       juce::Colour(0xff080c14), centreX + knobR * 0.5f, centreY + knobR, false);
        g.setGradientFill(knobGrad);
        g.fillEllipse(knobRect);
        g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.7f));
        g.drawEllipse(knobRect, 2.0f);

        // Inner dark cap
        g.setColour(juce::Colour(0xff05080e));
        g.fillEllipse(knobRect.reduced(5.0f));

        // Laser indicator line
        float pointerAngle = toAngle - juce::MathConstants<float>::halfPi;
        juce::Point<float> pStart(centreX + (knobR * 0.28f) * std::cos(pointerAngle),
                                  centreY + (knobR * 0.28f) * std::sin(pointerAngle));
        juce::Point<float> pEnd(centreX + (knobR - 4.0f) * std::cos(pointerAngle),
                                centreY + (knobR - 4.0f) * std::sin(pointerAngle));

        g.setColour(juce::Colour(0xffdcf5ff));
        g.drawLine(juce::Line<float>(pStart, pEnd), 3.0f);
    }
};

class AuraPitchAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::FileDragAndDropTarget,
                                      private juce::Timer
{
public:
    AuraPitchAudioProcessorEditor(AuraPitchAudioProcessor&);
    ~AuraPitchAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& suffix);
    void openAudioFileChooser();
    juce::String midiToNoteName(int midiNote) const;

    AuraPitchAudioProcessor& audioProcessor;
    CyberGlassLookAndFeel cyberLnf;

    juce::Slider centerKnob, driftKnob, vibratoKnob, transitionKnob, formantKnob;
    juce::Label centerLabel, driftLabel, vibratoLabel, transitionLabel, formantLabel;
    juce::ComboBox rootCombo, scaleCombo;

    juce::TextButton loadBtn { "IMPORT AUDIO" };
    juce::TextButton playBtn { "PLAY / PAUSE" };
    juce::TextButton stopBtn { "STOP" };
    juce::TextButton loopBtn { "LOOP: ON" };
    std::unique_ptr<juce::FileChooser> fileChooser;

    float displayedCenterMidi = 62.0f;
    bool logoLoaded = false;
    juce::Image logoImage;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> centerAtt, driftAtt, vibratoAtt, transitionAtt, formantAtt;
    std::unique_ptr<ComboAttachment> rootAtt, scaleAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraPitchAudioProcessorEditor)
};

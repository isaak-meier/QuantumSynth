#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class QuantumSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::OpenGLRenderer,
                                         private juce::Timer {

public:
    explicit QuantumSynthAudioProcessorEditor (QuantumSynthAudioProcessor&);
    ~QuantumSynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // OpenGLRenderer
    void newOpenGLContextCreated() override {}
    void renderOpenGL() override;
    void openGLContextClosing() override {}

    // Timer — repaints the 2D overlay so the detail readout stays live.
    void timerCallback() override;

private:
    juce::Rectangle<int> eyeBounds() const;   // the toggle hit area (bottom-right)

    static constexpr int keyboardHeight = 64;

    QuantumSynthAudioProcessor& processorRef;
    juce::OpenGLContext openGLContext;
    static constexpr int panelWidth = 120;

    juce::MidiKeyboardComponent keyboard;
    juce::Slider gainKnob;
    juce::Slider stiffnessKnob;
    juce::TextButton resetButton { "reset" };
    bool showDetail = false;

    // View fit, computed once from the rest geometry so vibration doesn't
    // re-normalise the camera (which pins the outermost atoms to the edges).
    bool fitted = false;
    float fitCx = 0.0f, fitCy = 0.0f, fitScale = 1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuantumSynthAudioProcessorEditor)
};

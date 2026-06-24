#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "MoleculaLookAndFeel.h"

class QuantumSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::OpenGLRenderer,
                                         private juce::Timer {

public:
    explicit QuantumSynthAudioProcessorEditor (QuantumSynthAudioProcessor&);
    ~QuantumSynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // OpenGLRenderer
    void newOpenGLContextCreated() override {}
    void renderOpenGL() override;
    void openGLContextClosing() override {}

    // Timer — repaints the 2D overlay so the detail readout stays live.
    void timerCallback() override;

private:
    juce::Rectangle<int> eyeBounds() const;   // the toggle hit area (bottom-right)
    juce::Point<float> worldToScreen (double wx, double wy) const;
    juce::Point<double> screenToWorld (juce::Point<int> p) const;
    void refreshMoleculeList();               // rescan Molecules/ into the dropdown
    void loadSelectedMolecule();              // load the .itp preset picked in the dropdown

    static constexpr int keyboardHeight = 64;

    MoleculaLookAndFeel lnf;   // declared first so it outlives the components using it
    QuantumSynthAudioProcessor& processorRef;
    juce::OpenGLContext openGLContext;
    static constexpr int panelWidth = 120;

    juce::MidiKeyboardComponent keyboard;
    juce::Slider gainKnob;
    juce::Slider stiffnessKnob;
    juce::Slider angleStiffnessKnob;
    juce::Slider massKnob;
    juce::Slider timestepKnob;
    juce::TextButton resetButton { "reset" };
    juce::TextButton kickButton { "kick" };
    juce::ComboBox moleculeBox;
    juce::ComboBox waveBox;
    juce::Array<juce::File> presetItps;       // one per dropdown item (item id - 1)
    juce::File currentMoleculeFolder;         // subfolder of the loaded preset, save target
    juce::ToggleButton phaseToggle { "phase" };
    juce::TextEditor filenameField;
    juce::TextButton saveButton { "save" };
    bool showDetail = false;

    // View fit, computed once from the rest geometry so vibration doesn't
    // re-normalise the camera (which pins the outermost atoms to the edges).
    bool fitted = false;
    float fitCx = 0.0f, fitCy = 0.0f, fitScale = 1.0f;
    int draggedAtom = -1;   // atom grabbed by the mouse, -1 = none
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuantumSynthAudioProcessorEditor)
};

#include "PluginEditor.h"
#include <cmath>

#ifndef QS_MOLECULES_DIR
 #define QS_MOLECULES_DIR "Molecules"   // CMake injects the real path
#endif

static juce::File moleculesDir() {
    return juce::File (QS_MOLECULES_DIR);
}

QuantumSynthAudioProcessorEditor::QuantumSynthAudioProcessorEditor (QuantumSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize (1200, 800);
    addAndMakeVisible (keyboard);

    gainKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainKnob.setRange (0.0, 1.0, 0.001);
    gainKnob.setValue (processorRef.outputGain.load(), juce::dontSendNotification);
    gainKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    gainKnob.setName ("gain");
    gainKnob.onValueChange = [this] {
        processorRef.outputGain.store ((float) gainKnob.getValue(), std::memory_order_relaxed);
    };
    addAndMakeVisible (gainKnob);

    stiffnessKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    stiffnessKnob.setRange (-99.0, 100.0, 1.0);                 // % change from file's k
    stiffnessKnob.setValue (0.0, juce::dontSendNotification);   // 0% = original stiffness
    stiffnessKnob.setTextValueSuffix ("%");
    stiffnessKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    stiffnessKnob.onValueChange = [this] {
        processorRef.setStiffnessPercent (stiffnessKnob.getValue());
    };
    addAndMakeVisible (stiffnessKnob);

    angleStiffnessKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    angleStiffnessKnob.setRange (-99.0, 100.0, 1.0);                 // % change from file's angle k
    angleStiffnessKnob.setValue (0.0, juce::dontSendNotification);
    angleStiffnessKnob.setTextValueSuffix ("%");
    angleStiffnessKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    angleStiffnessKnob.onValueChange = [this] {
        processorRef.setAngleStiffnessPercent (angleStiffnessKnob.getValue());
    };
    addAndMakeVisible (angleStiffnessKnob);

    massKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    massKnob.setRange (0.1, 10.0, 0.0);                 // 0 interval = continuous
    massKnob.setSkewFactorFromMidPoint (1.0);          // logarithmic: centre = 1.0x
    massKnob.setValue (1.0, juce::dontSendNotification);
    massKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    massKnob.onValueChange = [this] { processorRef.setMassScale (massKnob.getValue()); };
    addAndMakeVisible (massKnob);

    resetButton.onClick = [this] {
        processorRef.resetMolecule();   // back to pristine .gro coords, zero velocity
        // Defaults (sendNotification so the processor params update too).
        gainKnob.setValue (0.2, juce::sendNotification);
        stiffnessKnob.setValue (0.0, juce::sendNotification);
        angleStiffnessKnob.setValue (0.0, juce::sendNotification);
        massKnob.setValue (1.0, juce::sendNotification);
        fitted = false;                 // rescale the view to fit the molecule
        repaint();
    };
    addAndMakeVisible (resetButton);

    kickButton.onClick = [this] { processorRef.kickMolecule(); };
    addAndMakeVisible (kickButton);

    phaseToggle.setToggleState (processorRef.usePhase.load(), juce::dontSendNotification);
    phaseToggle.onClick = [this] {
        processorRef.usePhase.store (phaseToggle.getToggleState(), std::memory_order_relaxed);
    };
    addAndMakeVisible (phaseToggle);

    filenameField.setText ("out.itp", juce::dontSendNotification);
    addAndMakeVisible (filenameField);

    saveButton.onClick = [this] {
        if (! currentMoleculeFolder.isDirectory()) {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                "Save failed", "No molecule loaded");
            return;
        }
        auto name = filenameField.getText().trim();
        if (name.isEmpty()) name = "out.itp";
        if (! name.endsWithIgnoreCase (".itp")) name += ".itp";
        // New .itp preset in the current molecule's folder (shares its .gro).
        const auto file = currentMoleculeFolder.getChildFile (name);
        const auto err = processorRef.saveMolecule (file);
        if (err.isEmpty()) {
            refreshMoleculeList();                          // the new preset shows up in the list
            for (int i = 0; i < presetItps.size(); ++i)     // select it without reloading
                if (presetItps[i] == file)
                    moleculeBox.setSelectedItemIndex (i, juce::dontSendNotification);
        }
        juce::NativeMessageBox::showMessageBoxAsync (
            err.isEmpty() ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
            err.isEmpty() ? "Saved" : "Save failed",
            err.isEmpty() ? ("Wrote " + file.getFullPathName()) : err);
    };
    addAndMakeVisible (saveButton);

    // Molecule dropdown: one entry per .itp preset under Molecules/<molecule>/.
    refreshMoleculeList();
    for (int i = 0; i < presetItps.size(); ++i)               // reflect the default load
        if (presetItps[i].getFileName() == "CO2.itp") {
            moleculeBox.setSelectedItemIndex (i, juce::dontSendNotification);
            currentMoleculeFolder = presetItps[i].getParentDirectory();
            break;
        }
    moleculeBox.onChange = [this] { loadSelectedMolecule(); };
    addAndMakeVisible (moleculeBox);
    openGLContext.setRenderer (this);
    openGLContext.attachTo (*this);
    // ponytail: continuous repaint — cheap here and ready for the physics
    // animation to come; switch to triggerRepaint() if it's ever static + costly.
    openGLContext.setContinuousRepainting (true);
    startTimerHz (30);   // refresh the 2D detail overlay
}

void QuantumSynthAudioProcessorEditor::timerCallback()
{
    if (showDetail) repaint();   // re-render the live atom/bond readout
}

void QuantumSynthAudioProcessorEditor::refreshMoleculeList()
{
    moleculeBox.clear (juce::dontSendNotification);
    presetItps.clear();
    int id = 1;
    // Each .itp under Molecules/<molecule>/ is its own preset entry. Show just the
    // molecule name; disambiguate with the preset name only if a folder has several.
    for (const auto& folder : moleculesDir().findChildFiles (juce::File::findDirectories, false)) {
        const auto itps = folder.findChildFiles (juce::File::findFiles, false, "*.itp");
        for (const auto& itp : itps) {
            presetItps.add (itp);
            juce::String label = folder.getFileName();
            if (itps.size() > 1) label += " (" + itp.getFileNameWithoutExtension() + ")";
            moleculeBox.addItem (label, id++);
        }
    }
}

void QuantumSynthAudioProcessorEditor::loadSelectedMolecule()
{
    const int idx = moleculeBox.getSelectedItemIndex();
    if (idx < 0 || idx >= presetItps.size()) return;
    const auto itp = presetItps[idx];
    const auto folder = itp.getParentDirectory();
    const auto gros = folder.findChildFiles (juce::File::findFiles, false, "*.gro");

    const auto err = processorRef.loadMolecule (itp, gros.isEmpty() ? juce::File() : gros[0]);
    if (err.isNotEmpty())
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
            "Load failed", err);

    currentMoleculeFolder = folder;                                // save presets here
    fitted = false;                                                // re-fit to the new molecule
    stiffnessKnob.setValue (0.0, juce::dontSendNotification);       // knobs back to defaults
    angleStiffnessKnob.setValue (0.0, juce::dontSendNotification);
    massKnob.setValue (1.0, juce::dontSendNotification);
    repaint();
}

void QuantumSynthAudioProcessorEditor::resized()
{
    keyboard.setBounds (0, getHeight() - keyboardHeight, getWidth(), keyboardHeight);

    // Right-side control panel.
    const int px = getWidth() - panelWidth;
    const int kx = px + (panelWidth - 64) / 2;        // centre 64px-wide knobs
    gainKnob.setBounds (kx, 56, 64, 72);
    stiffnessKnob.setBounds (kx, 148, 64, 72);
    angleStiffnessKnob.setBounds (kx, 240, 64, 72);
    massKnob.setBounds (kx, 332, 64, 72);
    resetButton.setBounds (px + (panelWidth - 80) / 2, 424, 80, 26);
    kickButton.setBounds  (px + (panelWidth - 80) / 2, 456, 80, 26);
    moleculeBox.setBounds (px + 12, 500, panelWidth - 24, 24);     // label at 488
    phaseToggle.setBounds (px + 16, 532, panelWidth - 24, 24);
    filenameField.setBounds (px + 12, 578, panelWidth - 24, 22);   // label at 566
    saveButton.setBounds  (px + (panelWidth - 80) / 2, 606, 80, 26);
}

QuantumSynthAudioProcessorEditor::~QuantumSynthAudioProcessorEditor()
{
    openGLContext.detach();
}

void QuantumSynthAudioProcessorEditor::renderOpenGL()
{
    using namespace juce::gl;
    juce::OpenGLHelpers::clear (juce::Colour (0xff14101f));

    // The sim now runs on the audio thread (processBlock); here we only read
    // the current atom positions to draw them.
    const auto& mol = processorRef.molecule;
    const int n = (int) mol.atoms.size();
    if (n == 0) return;

    const float aspect = juce::jmax (1.0f, (float) getWidth()) / juce::jmax (1, getHeight());
    const float twoPi = juce::MathConstants<float>::twoPi;

    // Fit the view to the rest geometry ONCE. Re-fitting every frame would
    // re-normalise to the extremes and pin the outermost atoms in place.
    // ponytail: 2D — ignores z; orbit camera when z matters.
    if (! fitted) {
        float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f;
        for (const auto& atom : mol.atoms) {
            minX = juce::jmin (minX, (float) atom.x); maxX = juce::jmax (maxX, (float) atom.x);
            minY = juce::jmin (minY, (float) atom.y); maxY = juce::jmax (maxY, (float) atom.y);
        }
        fitCx = (minX + maxX) * 0.5f;
        fitCy = (minY + maxY) * 0.5f;
        fitScale = 1.2f / juce::jmax (maxX - minX, maxY - minY, 1.0f);  // leave headroom
        fitted = true;
    }
    const float cx = fitCx, cy = fitCy, scale = fitScale;

    // "Square" layout space: x is NOT pre-divided by aspect, so geometry stays
    // undistorted; the V() helper applies the aspect squeeze only at emit time.
    std::vector<juce::Point<float>> pos ((size_t) n);
    for (int i = 0; i < n; ++i) {
        const auto& atom = mol.atoms[(size_t) i];
        pos[(size_t) i] = { ((float) atom.x - cx) * scale, ((float) atom.y - cy) * scale };
    }

    const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto V = [&] (float x, float y) { glVertex2f (x / aspect, y); };
    auto disc = [&] (float ox, float oy, float rad, juce::Colour c) {
        constexpr int segs = 28;
        glColor4f (c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue(), c.getFloatAlpha());
        glBegin (GL_TRIANGLE_FAN);
        V (ox, oy);
        for (int k = 0; k <= segs; ++k) {
            const float a = twoPi * (float) k / (float) segs;
            V (ox + std::cos (a) * rad, oy + std::sin (a) * rad);
        }
        glEnd();
    };

    // Bonds as snakes: an undulating ribbon that slithers, with a little head.
    for (const auto& b : mol.bonds) {
        const auto i1 = (size_t) (b.atom1 - mol.atoms.data());
        const auto i2 = (size_t) (b.atom2 - mol.atoms.data());
        const auto a = pos[i1], c = pos[i2];
        const float dx = c.x - a.x, dy = c.y - a.y;
        const float L = std::sqrt (dx * dx + dy * dy);
        if (L < 1e-4f) continue;
        const float ux = dx / L, uy = dy / L;       // along bond
        const float px = -uy, py = ux;              // perpendicular
        const float amp = 0.07f, waves = 3.0f, phase = (float) t * 5.0f + (float) i1;
        const int N = 48;
        const float halfW = 0.022f;

        glBegin (GL_TRIANGLE_STRIP);
        for (int k = 0; k <= N; ++k) {
            const float s = (float) k / (float) N;
            const float env = std::sin (s * juce::MathConstants<float>::pi);   // 0 at ends
            const float off = amp * env * std::sin (s * waves * twoPi + phase);
            const float w = halfW * (0.35f + 0.65f * env);
            const float bx = a.x + dx * s + px * off;
            const float by = a.y + dy * s + py * off;
            const float scaleStripe = 0.55f + 0.45f * std::sin (s * 26.0f - (float) t * 6.0f);
            glColor4f (0.15f * scaleStripe, 0.55f + 0.35f * scaleStripe, 0.25f, 0.95f);
            V (bx + px * w, by + py * w);
            V (bx - px * w, by - py * w);
        }
        glEnd();

        // Head near atom2 end, with eyes and a flicking forked tongue.
        const float hs = 0.86f;
        const float hoff = amp * std::sin (juce::MathConstants<float>::pi * hs)
                               * std::sin (hs * waves * twoPi + phase);
        const float hx = a.x + dx * hs + px * hoff, hy = a.y + dy * hs + py * hoff;
        disc (hx, hy, 0.04f, juce::Colour::fromFloatRGBA (0.25f, 0.7f, 0.3f, 1.0f));
        disc (hx + px * 0.015f + ux * 0.012f, hy + py * 0.015f + uy * 0.012f, 0.008f, juce::Colours::black);
        disc (hx - px * 0.015f + ux * 0.012f, hy - py * 0.015f + uy * 0.012f, 0.008f, juce::Colours::black);
        const float flick = 0.012f * (0.5f + 0.5f * std::sin ((float) t * 18.0f));
        glColor4f (0.9f, 0.1f, 0.2f, 0.9f);
        glLineWidth (2.0f);
        glBegin (GL_LINES);
        V (hx + ux * 0.04f, hy + uy * 0.04f);
        V (hx + ux * (0.06f + flick) + px * 0.012f, hy + uy * (0.06f + flick) + py * 0.012f);
        V (hx + ux * 0.04f, hy + uy * 0.04f);
        V (hx + ux * (0.06f + flick) - px * 0.012f, hy + uy * (0.06f + flick) - py * 0.012f);
        glEnd();
    }

    // Atoms as plain discs, coloured by mass.
    for (int i = 0; i < n; ++i) {
        const auto& atom = mol.atoms[(size_t) i];
        const float r = 0.06f * (float) std::cbrt (juce::jmax (1.0, atom.mass));
        const float hue = juce::jlimit (0.0f, 0.7f,
            juce::jmap ((float) atom.mass, 1.0f, 40.0f, 0.7f, 0.0f));
        disc (pos[(size_t) i].x, pos[(size_t) i].y, r,
              juce::Colour::fromHSV (hue, 0.6f, 0.95f, 1.0f));
    }

    // Waveform scope across the lower area (full-width screen space, raw NDC).
    // Sits above the keyboard, which occupies the bottom strip.
    const float baseY = -0.55f, half = 0.11f, dispGain = 4.0f;
    glLineWidth (1.0f);
    glColor4f (0.25f, 0.30f, 0.40f, 0.5f);          // baseline
    glBegin (GL_LINES); glVertex2f (-1.0f, baseY); glVertex2f (1.0f, baseY); glEnd();

    const int W = QuantumSynthAudioProcessor::scopeSize;
    const int wpos = processorRef.scopeWrite.load (std::memory_order_relaxed);
    glLineWidth (1.5f);
    glColor4f (0.5f, 1.0f, 0.7f, 0.95f);
    glBegin (GL_LINE_STRIP);
    for (int i = 0; i < W; ++i) {
        const float x = -1.0f + 2.0f * (float) i / (float) (W - 1);
        const float v = juce::jlimit (-1.0f, 1.0f, processorRef.scope[(wpos + i) & (W - 1)] * dispGain);
        glVertex2f (x, baseY + v * half);
    }
    glEnd();
}

juce::Rectangle<int> QuantumSynthAudioProcessorEditor::eyeBounds() const
{
    return { getWidth() - 52, getHeight() - keyboardHeight - 32, 40, 26 };
}

void QuantumSynthAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (eyeBounds().contains (e.getPosition())) {
        showDetail = !showDetail;
        repaint();
    }
}

void QuantumSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    // GL renders the molecule underneath; label the molecule on top.
    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    g.drawText (processorRef.molecule.name,
                getLocalBounds().removeFromTop (40),
                juce::Justification::centred);

    // Right-side control panel background, then knob labels.
    const int px = getWidth() - panelWidth;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (px, 40, panelWidth, getHeight() - 40 - keyboardHeight);

    const int kx = px + (panelWidth - 64) / 2;
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (11.0f);
    g.drawText ("gain", kx, 44, 64, 12, juce::Justification::centred);
    g.drawText ("bond k", kx, 136, 64, 12, juce::Justification::centred);
    g.drawText ("angle k", kx, 228, 64, 12, juce::Justification::centred);
    g.drawText ("mass", kx, 320, 64, 12, juce::Justification::centred);
    g.drawText ("molecule", px + 12, 488, panelWidth - 24, 12, juce::Justification::centred);
    g.drawText ("save .itp as", px + 12, 564, panelWidth - 24, 12, juce::Justification::centred);

    // Eye toggle, bottom-right: an almond with a pupil; brighter when active.
    const auto eb = eyeBounds().toFloat();
    g.setColour (juce::Colours::white.withAlpha (showDetail ? 1.0f : 0.55f));
    juce::Path eye;
    eye.startNewSubPath (eb.getX(), eb.getCentreY());
    eye.quadraticTo (eb.getCentreX(), eb.getY(), eb.getRight(), eb.getCentreY());
    eye.quadraticTo (eb.getCentreX(), eb.getBottom(), eb.getX(), eb.getCentreY());
    eye.closeSubPath();
    g.strokePath (eye, juce::PathStrokeType (1.6f));
    g.fillEllipse (eb.getCentreX() - 4.0f, eb.getCentreY() - 4.0f, 8.0f, 8.0f);

    if (! showDetail) return;

    // Detail overlay: atom & bond properties.
    const auto& mol = processorRef.molecule;
    juce::StringArray lines;
    lines.add ("ATOMS");
    for (size_t i = 0; i < mol.atoms.size(); ++i) {
        const auto& a = mol.atoms[i];
        lines.add (juce::String::formatted ("  %d  m=%.1f  pos=(%.2f, %.2f, %.2f)",
                   (int) i, a.mass, a.x, a.y, a.z));
    }
    lines.add ("BONDS");
    for (size_t j = 0; j < mol.bonds.size(); ++j) {
        const auto& b = mol.bonds[j];
        const int i1 = (int) (b.atom1 - mol.atoms.data());
        const int i2 = (int) (b.atom2 - mol.atoms.data());
        lines.add (juce::String::formatted ("  %d-%d  k=%.2f  r0=%.2f",
                   i1, i2, b.k, b.eqBondLength));
    }
    lines.add ("ANGLES");
    for (const auto& ang : mol.angles) {
        const int i1 = (int) (ang.atom1 - mol.atoms.data());
        const int i2 = (int) (ang.atom2 - mol.atoms.data());   // vertex
        const int i3 = (int) (ang.atom3 - mol.atoms.data());
        const double th0 = ang.eqAngle * 180.0 / juce::MathConstants<double>::pi;
        lines.add (juce::String::formatted ("  %d-[%d]-%d  k=%.2f  th0=%.0f deg",
                   i1, i2, i3, ang.k, th0));
    }

    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                              13.0f, juce::Font::plain)));
    const int lh = 17, pad = 10, charW = 8;   // monospaced → width from char count
    int maxChars = 0;
    for (const auto& l : lines) maxChars = juce::jmax (maxChars, l.length());
    const int w = maxChars * charW;
    juce::Rectangle<int> panel (10, 48, w + pad * 2, (int) lines.size() * lh + pad * 2);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (panel.toFloat(), 6.0f);
    g.setColour (juce::Colours::white);
    for (int i = 0; i < lines.size(); ++i)
        g.drawText (lines[i], panel.getX() + pad, panel.getY() + pad + i * lh,
                    w, lh, juce::Justification::centredLeft);
}

#include "PluginEditor.h"
#include <cmath>

#ifndef QS_MOLECULES_DIR
 #define QS_MOLECULES_DIR "Molecules"   // CMake injects the real path
#endif

static juce::File moleculesDir() {
    return juce::File (QS_MOLECULES_DIR);
}

// Associated Legendre P_l^m(x), m >= 0 (Numerical Recipes recurrence).
static double plgndr (int l, int m, double x) {
    double pmm = 1.0;
    if (m > 0) {
        const double somx2 = std::sqrt ((1.0 - x) * (1.0 + x));
        double fact = 1.0;
        for (int i = 1; i <= m; ++i) { pmm *= -fact * somx2; fact += 2.0; }
    }
    if (l == m) return pmm;
    double pmmp1 = x * (2 * m + 1) * pmm;
    if (l == m + 1) return pmmp1;
    double pll = 0.0;
    for (int ll = m + 2; ll <= l; ++ll) {
        pll = (x * (2 * ll - 1) * pmmp1 - (ll + m - 1) * pmm) / (ll - m);
        pmm = pmmp1; pmmp1 = pll;
    }
    return pll;
}

// Real spherical harmonic angular shape (un-normalised; we rescale per atom).
static double realSH (int l, int m, double theta, double phi) {
    const int am = std::abs (m);
    const double p = plgndr (l, am, std::cos (theta));
    if (m > 0) return p * std::cos (am * phi);
    if (m < 0) return p * std::sin (am * phi);
    return p;
}

QuantumSynthAudioProcessorEditor::QuantumSynthAudioProcessorEditor (QuantumSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize (1200, 800);
    setLookAndFeel (&lnf);
    addAndMakeVisible (keyboard);

    // Each knob gets a distinct accent from the design's knob variants.
    gainKnob.setColour           (juce::Slider::rotarySliderFillColourId, juce::Colour (molecula::kPlasma));
    stiffnessKnob.setColour      (juce::Slider::rotarySliderFillColourId, juce::Colour (molecula::kPhosphor));
    angleStiffnessKnob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (molecula::kAmber));
    massKnob.setColour           (juce::Slider::rotarySliderFillColourId, juce::Colour (molecula::kDanger));
    timestepKnob.setColour       (juce::Slider::rotarySliderFillColourId, juce::Colour (molecula::kBond));

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

    timestepKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    timestepKnob.setRange (0.0, 30.0, 0.1);            // sim units/sec (0 = frozen)
    timestepKnob.setValue (processorRef.simSpeed.load(), juce::dontSendNotification);
    timestepKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    timestepKnob.onValueChange = [this] {
        processorRef.simSpeed.store ((float) timestepKnob.getValue(), std::memory_order_relaxed);
    };
    addAndMakeVisible (timestepKnob);

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

    // Depth buffer so the 3D spherical-harmonic atoms occlude correctly.
    juce::OpenGLPixelFormat pf;
    pf.depthBufferBits = 24;
    openGLContext.setPixelFormat (pf);

    // Oscillator waveform: item index 0..3 maps straight to the waveform enum.
    waveBox.addItemList ({ "Sine", "Triangle", "Sawtooth", "Square" }, 1);
    waveBox.setSelectedItemIndex (processorRef.waveform.load(), juce::dontSendNotification);
    waveBox.onChange = [this] {
        processorRef.waveform.store (waveBox.getSelectedItemIndex(), std::memory_order_relaxed);
    };
    addAndMakeVisible (waveBox);
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

    // Timestep dial sits top-left — it drives the simulation, not the sound.
    timestepKnob.setBounds (12, 50, 64, 72);          // label at 38

    // Right-side panel holds the sound/molecule controls.
    const int px = getWidth() - panelWidth;
    const int kx = px + (panelWidth - 64) / 2;        // centre 64px-wide knobs
    gainKnob.setBounds (kx, 56, 64, 72);
    stiffnessKnob.setBounds (kx, 148, 64, 72);
    angleStiffnessKnob.setBounds (kx, 240, 64, 72);
    massKnob.setBounds (kx, 332, 64, 72);
    resetButton.setBounds (px + (panelWidth - 80) / 2, 424, 80, 26);
    kickButton.setBounds  (px + (panelWidth - 80) / 2, 456, 80, 26);
    moleculeBox.setBounds (px + 12, 496, panelWidth - 24, 24);     // label at 484
    waveBox.setBounds     (px + 12, 536, panelWidth - 24, 24);     // label at 524
    phaseToggle.setBounds (px + 16, 566, panelWidth - 24, 24);
    filenameField.setBounds (px + 12, 612, panelWidth - 24, 22);   // label at 600
    saveButton.setBounds  (px + (panelWidth - 80) / 2, 640, 80, 26);
}

QuantumSynthAudioProcessorEditor::~QuantumSynthAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
    openGLContext.detach();
}

void QuantumSynthAudioProcessorEditor::renderOpenGL()
{
    using namespace juce::gl;
    juce::OpenGLHelpers::clear (juce::Colour (molecula::kVoid));
    glClear (GL_DEPTH_BUFFER_BIT);
    glDisable (GL_DEPTH_TEST);   // 2D layers (bonds, scope) draw without depth

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
            glColor4f (0.31f * scaleStripe, 0.76f * scaleStripe, 0.97f * scaleStripe, 0.95f);  // plasma shimmer
            V (bx + px * w, by + py * w);
            V (bx - px * w, by - py * w);
        }
        glEnd();

        // Head near atom2 end, with eyes and a flicking forked tongue.
        const float hs = 0.86f;
        const float hoff = amp * std::sin (juce::MathConstants<float>::pi * hs)
                               * std::sin (hs * waves * twoPi + phase);
        const float hx = a.x + dx * hs + px * hoff, hy = a.y + dy * hs + py * hoff;
        disc (hx, hy, 0.04f, juce::Colour (molecula::kPlasma));
        disc (hx + px * 0.015f + ux * 0.012f, hy + py * 0.015f + uy * 0.012f, 0.008f, juce::Colour (molecula::kVoid));
        disc (hx - px * 0.015f + ux * 0.012f, hy - py * 0.015f + uy * 0.012f, 0.008f, juce::Colour (molecula::kVoid));
        const float flick = 0.012f * (0.5f + 0.5f * std::sin ((float) t * 18.0f));
        glColor4f (1.0f, 0.29f, 0.42f, 0.9f);   // danger-red tongue
        glLineWidth (2.0f);
        glBegin (GL_LINES);
        V (hx + ux * 0.04f, hy + uy * 0.04f);
        V (hx + ux * (0.06f + flick) + px * 0.012f, hy + uy * (0.06f + flick) + py * 0.012f);
        V (hx + ux * 0.04f, hy + uy * 0.04f);
        V (hx + ux * (0.06f + flick) - px * 0.012f, hy + uy * (0.06f + flick) - py * 0.012f);
        glEnd();
    }

    // Atoms as spherical-harmonic surfaces: radius = |Y_l^m(theta,phi)|, coloured
    // by the harmonic value (blue -> teal -> yellow), spinning with a fixed tilt.
    // Manual 3D projection into layout space; GL depth test handles occlusion.
    glEnable (GL_DEPTH_TEST);
    static const int lm[][2] = { {3,2}, {4,2}, {5,1}, {2,1}, {6,3}, {4,3} };
    constexpr int Nt = 22, Np = 34;
    const float tilt = 0.45f;
    for (int i = 0; i < n; ++i) {
        const auto& atom = mol.atoms[(size_t) i];
        const int L = lm[i % 6][0], M = lm[i % 6][1];
        const float blob = 0.11f * (float) std::cbrt (juce::jmax (1.0, atom.mass));
        const float ox = pos[(size_t) i].x, oy = pos[(size_t) i].y;
        const float spin = (float) t * 0.6f + (float) i;
        const float cs = std::cos (spin), sn = std::sin (spin);
        const float ct = std::cos (tilt), st = std::sin (tilt);

        // Sample the harmonic onto a grid; track max |value| to normalise colour.
        std::vector<juce::Vector3D<float>> P ((size_t) ((Nt + 1) * (Np + 1)));
        std::vector<float> val ((size_t) ((Nt + 1) * (Np + 1)));
        float vmax = 1e-6f;
        auto idx = [&] (int a, int b) { return (size_t) (a * (Np + 1) + b); };
        for (int a = 0; a <= Nt; ++a) {
            const double th = juce::MathConstants<double>::pi * a / Nt;
            for (int b = 0; b <= Np; ++b) {
                const double ph = juce::MathConstants<double>::twoPi * b / Np;
                const double v = realSH (L, M, th, ph);
                const float rr = (float) std::abs (v) + 0.03f;       // tiny core so nodes don't vanish
                float lx = rr * (float) (std::sin (th) * std::cos (ph));
                float ly = rr * (float) std::cos (th);
                float lz = rr * (float) (std::sin (th) * std::sin (ph));
                const float x1 = lx * cs + lz * sn, z1 = -lx * sn + lz * cs;  // spin about y
                const float y2 = ly * ct - z1 * st, z2 = ly * st + z1 * ct;   // tilt about x
                P[idx (a, b)] = { x1, y2, z2 };
                val[idx (a, b)] = (float) v;
                vmax = juce::jmax (vmax, std::abs ((float) v));
            }
        }

        const float s = blob / vmax;   // normalise harmonic magnitude to blob size
        auto emit = [&] (size_t k) {
            const float tt = juce::jlimit (-1.0f, 1.0f, val[k] / vmax);
            const float shade = 0.55f + 0.45f * (juce::jlimit (-1.0f, 1.0f, P[k].z / vmax) * 0.5f + 0.5f);
            const auto col = juce::Colour::fromHSV (juce::jmap (tt, -1.0f, 1.0f, 0.66f, 0.13f), 0.85f, 0.95f * shade, 1.0f);
            glColor4f (col.getFloatRed(), col.getFloatGreen(), col.getFloatBlue(), 1.0f);
            glVertex3f ((ox + P[k].x * s) / aspect, oy + P[k].y * s, P[k].z * s);
        };
        glBegin (GL_QUADS);
        for (int a = 0; a < Nt; ++a)
            for (int b = 0; b < Np; ++b) {
                emit (idx (a, b)); emit (idx (a + 1, b));
                emit (idx (a + 1, b + 1)); emit (idx (a, b + 1));
            }
        glEnd();
    }
    glDisable (GL_DEPTH_TEST);

    // Waveform scope as concentric circular rings around the molecule: the
    // sample index sweeps the angle, the value pushes the radius. Many copies
    // out to the screen edge, each rotated by a small phase offset for a twist.
    // /aspect keeps them circular on screen.
    const int W = QuantumSynthAudioProcessor::scopeSize;
    const int wpos = processorRef.scopeWrite.load (std::memory_order_relaxed);
    const int rings = 13;
    const float rMin = 0.16f, dR = 0.11f, ringAmp = 0.05f, dispGain = 4.0f;
    const float phaseOffset = 0.18f;   // radians of rotation added per ring
    for (int ring = 0; ring < rings; ++ring) {
        const float R = rMin + dR * (float) ring;
        const float bright = juce::jmax (0.3f, 1.0f - 0.05f * (float) ring);

        glLineWidth (1.0f);                                 // faint baseline circle
        glColor4f (0.145f, 0.165f, 0.282f, 0.25f);
        glBegin (GL_LINE_LOOP);
        for (int k = 0; k < 96; ++k) {
            const float a = twoPi * (float) k / 96.0f;
            glVertex2f (std::cos (a) * R / aspect, std::sin (a) * R);
        }
        glEnd();

        glLineWidth (1.5f);                                 // the waveform itself
        glColor4f (0.66f * bright, 0.88f * bright, 0.39f * bright, 0.95f);
        glBegin (GL_LINE_LOOP);
        for (int i = 0; i < W; ++i) {
            const float a = twoPi * (float) i / (float) W + (float) ring * phaseOffset;
            const float v = juce::jlimit (-1.0f, 1.0f, processorRef.scope[(wpos + i) & (W - 1)] * dispGain);
            const float rad = R + v * ringAmp;
            glVertex2f (std::cos (a) * rad / aspect, std::sin (a) * rad);
        }
        glEnd();
    }
}

juce::Rectangle<int> QuantumSynthAudioProcessorEditor::eyeBounds() const
{
    return { getWidth() - 52, getHeight() - keyboardHeight - 32, 40, 26 };
}

// Same projection the renderer uses (fit + aspect; GL y points up).
juce::Point<float> QuantumSynthAudioProcessorEditor::worldToScreen (double wx, double wy) const
{
    const float aspect = juce::jmax (1.0f, (float) getWidth()) / juce::jmax (1, getHeight());
    const float ndcX = ((float) wx - fitCx) * fitScale / aspect;
    const float ndcY = ((float) wy - fitCy) * fitScale;
    return { (ndcX * 0.5f + 0.5f) * getWidth(), (0.5f - ndcY * 0.5f) * getHeight() };
}

juce::Point<double> QuantumSynthAudioProcessorEditor::screenToWorld (juce::Point<int> p) const
{
    const float aspect = juce::jmax (1.0f, (float) getWidth()) / juce::jmax (1, getHeight());
    const float ndcX = (float) p.x / getWidth() * 2.0f - 1.0f;
    const float ndcY = 1.0f - (float) p.y / getHeight() * 2.0f;
    return { (double) (ndcX * aspect / fitScale + fitCx), (double) (ndcY / fitScale + fitCy) };
}

void QuantumSynthAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (eyeBounds().contains (e.getPosition())) {
        showDetail = !showDetail;
        repaint();
        return;
    }

    // Grab the nearest atom within ~45px of the click.
    const auto& mol = processorRef.molecule;
    int best = -1;
    float bestD = 45.0f;
    for (size_t i = 0; i < mol.atoms.size(); ++i) {
        const float d = worldToScreen (mol.atoms[i].x, mol.atoms[i].y).getDistanceFrom (e.position);
        if (d < bestD) { bestD = d; best = (int) i; }
    }
    if (best >= 0) {
        draggedAtom = best;
        processorRef.setHeldAtom (best, mol.atoms[(size_t) best].x, mol.atoms[(size_t) best].y);
    }
}

void QuantumSynthAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedAtom < 0) return;
    const auto w = screenToWorld (e.getPosition());
    processorRef.setHeldAtom (draggedAtom, w.x, w.y);
}

void QuantumSynthAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (draggedAtom >= 0) {
        processorRef.clearHeldAtom();
        draggedAtom = -1;
    }
}

void QuantumSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    // GL renders the molecule underneath; label the molecule on top.
    g.setColour (juce::Colour (molecula::kBond));
    g.setFont (lnf.displayFont (24.0f));
    g.drawText (processorRef.molecule.name,
                getLocalBounds().removeFromTop (40),
                juce::Justification::centred);

    // Right-side control panel background, with a plasma rim down its left edge.
    const int px = getWidth() - panelWidth;
    const int panelH = getHeight() - 40 - keyboardHeight;
    g.setColour (juce::Colour (molecula::kPanel));
    g.fillRect (px, 40, panelWidth, panelH);
    g.setColour (juce::Colour (molecula::kRim));
    g.fillRect (px, 40, 1, panelH);

    const int kx = px + (panelWidth - 64) / 2;
    g.setColour (juce::Colour (molecula::kMuted));
    g.setFont (lnf.monoFont (11.0f));
    g.drawText ("timestep", 12, 38, 64, 12, juce::Justification::centred);   // top-left, sim control
    g.drawText ("gain", kx, 44, 64, 12, juce::Justification::centred);
    g.drawText ("bond k", kx, 136, 64, 12, juce::Justification::centred);
    g.drawText ("angle k", kx, 228, 64, 12, juce::Justification::centred);
    g.drawText ("mass", kx, 320, 64, 12, juce::Justification::centred);
    g.drawText ("molecule", px + 12, 484, panelWidth - 24, 12, juce::Justification::centred);
    g.drawText ("waveform", px + 12, 524, panelWidth - 24, 12, juce::Justification::centred);
    g.drawText ("save .itp as", px + 12, 600, panelWidth - 24, 12, juce::Justification::centred);

    // Eye toggle, bottom-right: an almond with a pupil; brighter when active.
    const auto eb = eyeBounds().toFloat();
    g.setColour (juce::Colour (molecula::kPlasma).withAlpha (showDetail ? 1.0f : 0.55f));
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

    g.setFont (lnf.monoFont (13.0f));
    const int lh = 17, pad = 10, charW = 8;   // monospaced → width from char count
    int maxChars = 0;
    for (const auto& l : lines) maxChars = juce::jmax (maxChars, l.length());
    const int w = maxChars * charW;
    juce::Rectangle<int> panel (10, 132, w + pad * 2, (int) lines.size() * lh + pad * 2);  // below the timestep knob
    g.setColour (juce::Colour (molecula::kGlass).withAlpha (0.92f));
    g.fillRoundedRectangle (panel.toFloat(), 8.0f);
    g.setColour (juce::Colour (molecula::kRim));
    g.drawRoundedRectangle (panel.toFloat(), 8.0f, 1.0f);
    for (int i = 0; i < lines.size(); ++i) {
        const bool header = ! lines[i].startsWithChar (' ');   // section title vs data row
        g.setColour (juce::Colour (header ? molecula::kPlasma : molecula::kBond));
        g.drawText (lines[i], panel.getX() + pad, panel.getY() + pad + i * lh,
                    w, lh, juce::Justification::centredLeft);
    }
}

---
name: run-quantumsynth
description: Build and launch the QuantumSynth standalone synth app. Use for /run, or whenever building/relaunching the app to see a change.
---

# Build & run QuantumSynth

A JUCE plugin; the **Standalone** target is the app to run. From the repo root:

```sh
cmake --build build --target QuantumSynth_Standalone
pkill -f "QuantumSynth_artefacts/Standalone" 2>/dev/null; sleep 1
open "build/QuantumSynth_artefacts/Standalone/QuantumSynth.app"
sleep 2; pgrep -lf "QuantumSynth_artefacts/Standalone"   # confirm it launched (PID)
```

Notes:
- **First build, or after editing `CMakeLists.txt`:** run `cmake -S . -B build` first (downloads JUCE on the first run — slow), then the build command above.
- The app finds molecule presets in `Molecules/<name>/` via a path baked in by CMake; switch presets with the on-screen dropdown.
- **Do not screenshot** to verify — the user checks the GUI themselves. Report the build result and PID.
- **Tests** are standalone `.cpp` files in `Test/`; compile and run one directly, e.g.
  `cd Test && c++ -std=c++17 test_sound.cpp -o /tmp/t && /tmp/t`. Run the relevant test after changing parser/forces/integrator/sound code.

Synth that uses molecules to create sound

Implements simple molecular dynamics engine

## Build

Requires CMake (3.22+), a C++17 compiler, and an internet connection (JUCE is
fetched automatically on first configure).

```sh
cmake -S . -B build          # first run downloads JUCE — slow
cmake --build build          # builds VST3, AU, and Standalone
```

Run the standalone app:

```sh
open "build/QuantumSynth_artefacts/Standalone/QuantumSynth.app"   # macOS
```

The `Molecules/` folder (one subfolder per molecule, each with a `.itp` and
`.gro`) is located via a path baked in at configure time, so the app finds it
from any checkout. In the standalone, open **Options → Audio/MIDI Settings** to
pick an output device, then play the on-screen keyboard.


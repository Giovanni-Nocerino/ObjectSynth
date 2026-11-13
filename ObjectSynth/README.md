# ObjectSynth — JUCE starter

A minimal, intentionally **silent** C++17 synthesizer plugin scaffold.

- macOS / Apple Clang
- VS Code + CMake Tools
- JUCE **9.0.5** (automatically downloaded by CMake on first configure)
- Standalone + VST3
- MIDI input enabled

## Prerequisites

In Terminal:

```bash
xcode-select --install   # skip if Apple Command Line Tools already installed
brew install cmake ninja # assumes Homebrew is installed
```

VS Code extensions: `ms-vscode.cpptools`, `ms-vscode.cmake-tools`.

## Build using the terminal

From this project's root:

```bash
cmake --preset debug
cmake --build --preset debug --target ObjectSynth_Standalone
cmake --build --preset debug --target ObjectSynth_VST3
```

The first configure automatically fetches JUCE via Git, which needs an internet connection.

Find the output bundles:

```bash
find build/debug -name 'ObjectSynth.app' -o -name 'ObjectSynth.vst3'
```

Open the `.app` with Finder, or:

```bash
open "$(find build/debug -type d -name ObjectSynth.app -print -quit)"
```

## Build using VS Code

1. File > Open Folder... > this folder.
2. Command Palette (`Cmd+Shift+P`) > **CMake: Select Configure Preset** > `debug`.
3. Command Palette > **CMake: Configure** (downloads JUCE on first use).
4. Command Palette > **CMake: Set Build Target** > `ObjectSynth_Standalone`.
5. Command Palette > **CMake: Build**.

Use the VST3 build target to build the plugin bundle.

## Architecture

- `Source/PluginProcessor.*`: audio processing + MIDI entrypoint (currently clears the output buffer).
- `Source/PluginEditor.*`: basic GUI (should display **ObjectSynth**).
- `CMakeLists.txt`: JUCE dependency, plugin settings and C++ modules.
- `CMakePresets.json`: Debug/Release build configuration.

## First milestone

This build should show a window that says **ObjectSynth**. It is intentionally silent: next we will implement the wavetable oscillator and wire it into `processBlock()`.

## Note on licensing

JUCE is available under AGPLv3 and commercial JUCE terms. Check the applicable licence before public distribution.

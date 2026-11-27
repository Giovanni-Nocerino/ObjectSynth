# ObjectSynth

Photograph an object, turn its shape into a wavetable synth sound.

- `CV/`: the camera pipeline (camera -> SAM mask -> wavetable + macros -> `.ospreset`).
- `ObjectSynth/`: the JUCE wavetable synth (VST3 + Standalone) that plays the presets
  and has a **FROM CAMERA** button that runs the pipeline.
- `docs/macro-map.html`: an interactive page that shows how it all works, with
  animations, knobs and sound (see [Explorer page](#explorer-page)).

The camera button opens the pipeline in a terminal window: Terminal on macOS,
a command prompt on Windows, your terminal emulator on Linux (`$TERMINAL`, or the
first of x-terminal-emulator, gnome-terminal, konsole, xfce4-terminal, kitty,
alacritty, xterm). The pipeline itself is plain Python.

## 1. Python environment (needed for the camera)

The plugin looks for Python in a `.venv` folder at the **root of this repository**
(`.venv/bin/python` on macOS and Linux, `.venv\Scripts\python.exe` on Windows),
so create it exactly there. Use Python 3.12.

macOS / Linux:

```bash
cd ObjectSynth
python3.12 -m venv .venv
.venv/bin/pip install -r CV/requirements.txt
```

Fedora ships a newer Python by default; install 3.12 next to it with
`sudo dnf install python3.12`. On Linux without an NVIDIA GPU, install the CPU-only
PyTorch **before** the line above: the default one downloads several GB of CUDA
libraries that are of no use there.

```bash
.venv/bin/pip install torch==2.14.1 torchvision==0.29.1 --index-url https://download.pytorch.org/whl/cpu
```

Windows (PowerShell):

```powershell
cd ObjectSynth
py -3.12 -m venv .venv
.venv\Scripts\pip install -r CV\requirements.txt
```

## 2. SAM model

The pipeline uses `CV/models/sam2.1_b.pt` (about 160 MB). Model files are not in
git. Ultralytics downloads it to that path the first time you segment an object
(internet needed once), or download it yourself from

<https://github.com/ultralytics/assets/releases/download/v8.4.0/sam2.1_b.pt>

and save it as `CV/models/sam2.1_b.pt` (create the `models` folder if needed):

```bash
mkdir -p CV/models
curl -L -o CV/models/sam2.1_b.pt https://github.com/ultralytics/assets/releases/download/v8.4.0/sam2.1_b.pt
```

## 3. Build the plugin

The plugin remembers where this repository is **at build time** (to find `CV/` and
`.venv`). Build it on your own machine from your own checkout; a plugin built on
someone else's computer points to their folders and its camera button will not work.

Prerequisites: a C++17 compiler (Xcode Command Line Tools on macOS, Visual Studio
on Windows, GCC/Clang plus JUCE's Linux packages on Linux), CMake and Ninja
(macOS: `brew install cmake ninja`). On Fedora, everything in one line:

```bash
sudo dnf install gcc-c++ cmake ninja-build pkgconf alsa-lib-devel freetype-devel fontconfig-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXext-devel
```

```bash
cd ObjectSynth
cmake --preset release
cmake --build --preset release --target ObjectSynth_VST3 ObjectSynth_Standalone
```

The first configure downloads JUCE 9.0.5 (internet needed).

**No DAW? Use the standalone app.** ObjectSynth also builds as a normal desktop app
with its own audio settings and an on-screen keyboard (a MIDI keyboard works too).
To build only that, use `--target ObjectSynth_Standalone`. It is in
`build/release/ObjectSynth_artefacts/Release/Standalone/`:

- macOS: `open build/release/ObjectSynth_artefacts/Release/Standalone/ObjectSynth.app`
- Windows: `build\release\ObjectSynth_artefacts\Release\Standalone\ObjectSynth.exe`
- Linux: `./build/release/ObjectSynth_artefacts/Release/Standalone/ObjectSynth`

The plugin is in
`build/release/ObjectSynth_artefacts/Release/VST3/ObjectSynth.vst3`; to use it in a
DAW, copy it to the VST3 folder:

- macOS: `~/Library/Audio/Plug-Ins/VST3/`
- Windows: `C:\Program Files\Common Files\VST3\`
- Linux: `~/.vst3/`

If you move or rename the repository folder, build again.

## 4. Camera permission

- macOS: the first time you press **FROM CAMERA**, macOS asks whether Terminal may
  use the camera: allow it. If you said no, enable Terminal in
  System Settings > Privacy & Security > Camera, then try again.
- Windows: Settings > Privacy & security > Camera: allow desktop apps to use the camera.
- Linux: your user needs access to `/dev/video0` (usually the `video` group).

## Using the camera

Press **FROM CAMERA** in the oscillator panel. A terminal window opens:

1. Camera preview: **Space** takes the photo, **Enter** keeps it (**R** retakes).
2. Left click the object, right click the hand or background to exclude, then
   **Enter** to segment. Add points and press **Enter** again to refine,
   **S** to keep the mask (**R** clears the points).
3. Type the object's name in the terminal (Enter alone uses the photo's name).

The preset is saved in `camera-acquired-presets/` and loaded into the synth. Errors are shown
under the oscillator and in the terminal window.

You can also run the pipeline without the plugin:

```bash
cd CV
../.venv/bin/python run_pipeline.py
```

and open the preset with **OPEN PRESET**.

## Explorer page

`docs/macro-map.html` is a visual explanation of the project, in three tabs. Pick an
object from the **Object** menu at the top and every tab follows it:
- **Wavetable conversion** animates, step by step as you scroll inside each panel, how a
  photo becomes the wavetable: the columns collapsing into a curve and its 64 harmonics,
  and the rings of the 2D spectrum. A slider plays the 32 frames.
- **Macro implementation** shows how each macro is measured and which synth parameters it
  turns, with knobs you can move.
- **Effects** plays the object's wavetable through the synth's chain (same algorithms and
  parameters, rebuilt with Web Audio) with the settings its macros chose, and shows each
  module on an oscilloscope, a spectrum, a stereo field and an envelope trace.

There are two ways to open it.

**Just the page (no setup).** Open `docs/macro-map.html` in any browser, e.g. by
double-clicking it. Everything works with the 29 example objects (photos from the
dataset, already inside the page): animations, knobs and sound. Only **TRY YOUR OWN!**
needs the server below; without it, the lens shows the command to start it.

**With the server (your own photos and the camera).** The **TRY YOUR OWN!** lens (top
right) runs your photo through the same SAM, wavetable and macro code as the pipeline.
It needs the Python environment from step 1 and a small local server. From the
repository folder:

```bash
.venv/bin/python CV/macro_server.py
```

On Windows: `.venv\Scripts\python CV\macro_server.py`. Then open
<http://127.0.0.1:8765> (open it from this address rather than from the file: browsers
only allow the camera on a page served like this). Press the lens, upload a photo or
take one, click the object (right-click or **− Background** to exclude parts) and press
**Measure macros**: "★ your object" appears in the Object menu and in every tab. The
first photo takes a few seconds while SAM loads. Stop the server with Ctrl+C.

## Troubleshooting

Linux note: we developed and tested on macOS. The Linux build and camera button follow
the same code but were not tried on a real Linux machine; Fedora was checked against its
documentation (packages, Ptyxis, Wayland). The explorer page works everywhere.

| Message | Fix |
| --- | --- |
| `Python not found: …` | Step 1, at the repository root; or the plugin was built from another checkout (step 3). |
| `CV pipeline not found: …` | The plugin was built from another folder: build it again from this checkout. |
| `ModuleNotFoundError` in Terminal | `.venv/bin/pip install -r CV/requirements.txt` |
| Black or no camera image | Allow the camera (step 4). |
| `No terminal found` (Linux) | Set `$TERMINAL` to your terminal, or install xterm. Ptyxis (Fedora), GNOME Terminal, Konsole, xfce4-terminal, kitty and Alacritty are found by themselves. |
| `Could not load the Qt platform plugin "xcb"` (Linux) | The camera windows need XWayland's xcb plugin: `sudo dnf install xcb-util-cursor` (Fedora) or `sudo apt install libxcb-cursor0` (Ubuntu). |
| No sound from the standalone app (Linux) | Open its **Options** > audio settings and pick the output device. JUCE talks to ALSA, which PipeWire provides on Fedora. |
| Download error while segmenting | Get the SAM model with internet on (step 2). |

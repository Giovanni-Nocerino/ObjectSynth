# PlayAnyThing

Photograph an object, turn its shape into a wavetable synth sound.

- `CV/`: the camera pipeline (camera -> SAM mask -> wavetable + macros -> `.ospreset`).
- `ObjectSynth/`: the JUCE wavetable synth (VST3 + Standalone) that plays the presets
  and has a **FROM CAMERA** button that runs the pipeline.

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
cd PlayAnyThingVST
python3.12 -m venv .venv
.venv/bin/pip install -r CV/requirements.txt
```

Windows (PowerShell):

```powershell
cd PlayAnyThingVST
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
(macOS: `brew install cmake ninja`).

```bash
cd ObjectSynth
cmake --preset release
cmake --build --preset release --target ObjectSynth_VST3 ObjectSynth_Standalone
```

The first configure downloads JUCE 9.0.5 (internet needed). The plugin is in
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

## Macro map

`docs/macro-map.html` shows how each macro is measured and which synth parameters it
turns; open it in any browser. Its **TRY YOUR OWN!** lens measures your own photo with
the same SAM and macro code as the pipeline, through a small local server:

```bash
.venv/bin/python CV/macro_server.py
```

Then open <http://127.0.0.1:8765> (the camera works there in every browser), press the
lens, upload a photo or take one, click the object and press **Measure macros**.

## Troubleshooting

| Message | Fix |
| --- | --- |
| `Python not found: …` | Step 1, at the repository root; or the plugin was built from another checkout (step 3). |
| `CV pipeline not found: …` | The plugin was built from another folder: build it again from this checkout. |
| `ModuleNotFoundError` in Terminal | `.venv/bin/pip install -r CV/requirements.txt` |
| Black or no camera image | Allow the camera (step 4). |
| `No terminal found` (Linux) | Set `$TERMINAL` to your terminal, or install xterm. |
| Download error while segmenting | Get the SAM model with internet on (step 2). |

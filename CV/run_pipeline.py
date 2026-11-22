"""Capture, segment, and export an alpha-scanned Serum wavetable and an
ObjectSynth preset (.ospreset) with the wavetable and the macro mapping.

Usage:
  python run_pipeline.py
  python run_pipeline.py --result-file /path/result.txt   # used by the plugin

With --result-file the outcome is written there when the pipeline ends, so
ObjectSynth's CAMERA button can load the preset: first line OK, ERROR or
CANCELLED; for OK the second line is the preset's path, otherwise a message.
"""

import argparse
import os
from pathlib import Path

from acquisition import capture_image
from make_presets import make_preset, set_object_name
from recognition import segment_image
from to_wav import export_wavetable
from waveforom_extraction import extract_wavetable_features


class PipelineStopped(Exception):
    """The user closed a window or a step produced nothing."""


def ask_object_name(capture_stem):
    """Optional human name for the preset; Enter keeps the capture's file name."""
    try:
        name = input("Nome dell'oggetto per il preset (Invio = nome della foto): ")
    except EOFError:
        name = ""
    return set_object_name(capture_stem, name)


def run():
    """Run every step; return the preset path or raise PipelineStopped."""
    image_path = capture_image()
    if image_path is None:
        raise PipelineStopped("No photo saved.")

    mask_path = segment_image(image_path)
    if mask_path is None:
        raise PipelineStopped("No mask saved.")

    try:
        features_path = extract_wavetable_features(image_path, mask_path)
        export_wavetable(features_path)
    except (FileNotFoundError, ValueError, OSError) as error:
        raise RuntimeError(f"Wavetable export failed: {error}") from error

    # ObjectSynth preset: same wavetable + parameters from the four macros,
    # normalised with the stored calibration (macro_calibration.json).
    try:
        image_path, mask_path = Path(image_path), Path(mask_path)
        ask_object_name(image_path.stem)
        preset_path, values, _ = make_preset(image_path, mask_path)
    except (FileNotFoundError, ValueError, OSError) as error:
        raise RuntimeError(f"Preset export failed: {error}") from error

    print(f"ObjectSynth preset saved to: {preset_path}")
    print(f"Spigolosita {values['sharpness']:.2f} | Luminosita {values['brightness']:.2f} | "
          f"Corpo {values['body']:.2f} | Movimento {values['movement']:.2f}")
    return preset_path


def write_result(result_file, status, detail):
    """Write the outcome atomically, so the plugin never reads half a file."""
    result_file = Path(result_file)
    partial = result_file.with_name(result_file.name + ".partial")
    partial.write_text(f"{status}\n{detail}\n", encoding="utf-8")
    os.replace(partial, result_file)


def main():
    parser = argparse.ArgumentParser(description="PlayAnyThing camera pipeline.")
    parser.add_argument("--result-file", type=Path,
                        help="Write OK/ERROR/CANCELLED and the preset path here.")
    args = parser.parse_args()

    status, detail = "ERROR", "The pipeline stopped unexpectedly."
    try:
        preset_path = run()
        status, detail = "OK", str(Path(preset_path).resolve())
        print("Open it in ObjectSynth with OPEN PRESET." if not args.result_file
              else "Preset sent to ObjectSynth. You can close this window.")
    except PipelineStopped as stop:
        status, detail = "CANCELLED", str(stop)
        print(f"{stop} Pipeline stopped.")
    except Exception as error:  # report anything to the plugin, then re-raise
        status, detail = "ERROR", str(error)
        print(error)
        if not args.result_file:
            raise
    finally:
        if args.result_file:
            write_result(args.result_file, status, detail)


if __name__ == "__main__":
    main()

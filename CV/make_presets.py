"""Generate ObjectSynth .ospreset files from the saved captures and masks.

For every capture with a SAM mask:
  1. build the 32-frame wavetable (same features as the Serum export),
  2. measure the macros (Spigolosita, Luminosita, Corpo, Movimento),
  3. map the macros to synth parameters,
  4. write presets/<object>.ospreset with wavetable + parameters embedded
     (object names come from object_names.json; unnamed captures keep their file name).

Usage:
  python make_presets.py              # all saved masks, recalibrates first
  python make_presets.py --no-calibrate
"""

import argparse
import json

import macros
from macro_mapping import preset_parameters
from ospreset import write_preset
from to_wav import build_wavetable
from waveforom_extraction import (
    FRAME_COUNT, HARMONIC_COUNT, CAPTURE_DIRECTORY, MASK_DIRECTORY,
    luminance_profile, profile_to_coefficients, radial_fft_amplitudes, read_masked_crop,
)
import numpy as np


PRESET_DIRECTORY = macros.CV_DIRECTORY / "presets"
OBJECT_NAMES_PATH = macros.CV_DIRECTORY / "object_names.json"


def _object_names():
    if OBJECT_NAMES_PATH.is_file():
        return json.loads(OBJECT_NAMES_PATH.read_text())
    return {}


def object_name(capture_stem):
    """Human name of the photographed object (object_names.json), or the capture name."""
    return _object_names().get(capture_stem, capture_stem)


def set_object_name(capture_stem, name):
    """Remember the object's name for a capture; returns the name actually stored."""
    name = " ".join(name.replace("/", "-").replace("\\", "-").split())
    if not name:
        return capture_stem
    names = _object_names()
    names[capture_stem] = name
    OBJECT_NAMES_PATH.write_text(json.dumps(names, indent=2, ensure_ascii=False) + "\n")
    return name


def saved_pairs():
    """(capture image, mask) for every capture that has a mask."""
    pairs = []
    for mask_path in sorted(MASK_DIRECTORY.glob("capture_*_mask.png")):
        image_path = CAPTURE_DIRECTORY / (mask_path.name.removesuffix("_mask.png") + ".jpg")
        if image_path.is_file():
            pairs.append((image_path, mask_path))
    return pairs


def wavetable_frames(image_path, mask_path):
    """The 32 wavetable frames, computed in memory (no files written)."""
    masked_luminance, _ = read_masked_crop(image_path, mask_path)
    data = {
        "harmonic_count": HARMONIC_COUNT,
        "frame_count": FRAME_COUNT,
        "alpha_values": np.linspace(0.0, 1.0, FRAME_COUNT).tolist(),
        "shape_coefficients": profile_to_coefficients(luminance_profile(masked_luminance)),
        "texture_amplitudes": radial_fft_amplitudes(masked_luminance),
    }
    return build_wavetable(data).reshape(FRAME_COUNT, -1)


def make_preset(image_path, mask_path):
    values = macros.all_macros(image_path, mask_path)
    parameters = preset_parameters(values)
    frames = wavetable_frames(image_path, mask_path)
    name = object_name(image_path.stem)
    path = write_preset(
        PRESET_DIRECTORY / f"{name}.ospreset", parameters,
        frames=frames, wavetable_name=name, frame_index=0,
    )
    return path, values, parameters


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--no-calibrate", action="store_true",
                        help="Keep the stored calibration instead of refitting it.")
    args = parser.parse_args()

    pairs = saved_pairs()
    if not pairs:
        print(f"No capture/mask pairs found in {CAPTURE_DIRECTORY} and {MASK_DIRECTORY}.")
        return

    if not args.no_calibrate:
        measurements = [macros.raw_values(image, mask) for image, mask in pairs]
        for name in measurements[0]:
            entry = macros.calibrate(name, [m[name] for m in measurements])
            print(f"{name} calibrated on {entry['samples']} captures "
                  f"(raw {entry['min']:.3f} .. {entry['max']:.3f})")

    for image_path, mask_path in pairs:
        try:
            path, values, parameters = make_preset(image_path, mask_path)
        except (ValueError, OSError) as error:
            print(f"{image_path.stem}: skipped ({error})")
            continue
        print(f"{path.name}: spigolosita={values['sharpness']:.2f} "
              f"luminosita={values['brightness']:.2f} corpo={values['body']:.2f} "
              f"movimento={values['movement']:.2f} (asimmetria={values['asymmetry']:.2f} "
              f"texture={values['texture']:.2f} colore={values['colorfulness']:.2f})")


if __name__ == "__main__":
    main()

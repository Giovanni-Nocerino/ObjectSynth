"""Generate ObjectSynth .ospreset files from the saved captures and masks.

For every capture with a SAM mask:
  1. build the 32-frame wavetable (same features as the Serum export),
  2. measure the macros (sharpness, brightness, body, movement),
  3. map the macros to synth parameters,
  4. write presets/<object>.ospreset with wavetable + parameters embedded
     (object names come from object_names.json; unnamed captures keep their file name).

Usage:
  python make_presets.py

The macros are mapped to 0..1 with the fixed ranges in macros.MACRO_RANGES.
"""

import json

import cv2

import macros
from macro_mapping import preset_parameters
from ospreset import write_preset
from to_wav import build_wavetable
from waveforom_extraction import (
    FRAME_COUNT, HARMONIC_COUNT, CAPTURE_DIRECTORY, MASK_DIRECTORY,
    luminance_profile, profile_to_coefficients, radial_fft_amplitudes, read_masked_crop,
)
import numpy as np


# Batch output of this script: local only (git-ignored).
PRESET_DIRECTORY = macros.CV_DIRECTORY / "presets"
# Presets taken with the camera (run_pipeline.py / the synth's camera button):
# kept in git, at the repository root.
CAMERA_PRESET_DIRECTORY = macros.CV_DIRECTORY.parent / "camera-acquired-presets"
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


PHOTO_LONG_SIDE = 320      # thumbnail size in pixels (long side)
PHOTO_MARGIN = 0.12        # space around the object, as a share of its size
PHOTO_JPEG_QUALITY = 85


def object_photo(image_path, mask_path):
    """JPEG thumbnail of the object: cropped around the SAM mask, background
    darkened, mask outline drawn, so the synth shows what was photographed."""
    image = cv2.imread(str(image_path))
    mask = cv2.imread(str(mask_path), cv2.IMREAD_GRAYSCALE)
    if image is None or mask is None:
        return None
    mask = cv2.resize(mask, (image.shape[1], image.shape[0]), interpolation=cv2.INTER_NEAREST) > 127
    if not mask.any():
        return None

    ys, xs = np.nonzero(mask)
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    margin = int(max(x1 - x0, y1 - y0) * PHOTO_MARGIN)
    x0, y0 = max(0, x0 - margin), max(0, y0 - margin)
    x1, y1 = min(image.shape[1], x1 + margin), min(image.shape[0], y1 + margin)
    # 4:3 like a camera frame, centred on the object.
    width, height = x1 - x0, y1 - y0
    if width < height * 4 / 3:
        extra = int(height * 4 / 3 - width) // 2
        x0, x1 = max(0, x0 - extra), min(image.shape[1], x1 + extra)
    else:
        extra = int(width * 3 / 4 - height) // 2
        y0, y1 = max(0, y0 - extra), min(image.shape[0], y1 + extra)

    crop = image[y0:y1, x0:x1].astype(np.float32)
    crop_mask = mask[y0:y1, x0:x1]
    crop[~crop_mask] *= 0.35
    crop = crop.clip(0, 255).astype(np.uint8)
    contours, _ = cv2.findContours(crop_mask.astype(np.uint8), cv2.RETR_EXTERNAL,
                                   cv2.CHAIN_APPROX_NONE)
    scale = PHOTO_LONG_SIDE / max(crop.shape[:2])
    cv2.drawContours(crop, contours, -1, (235, 235, 235), max(1, round(1.5 / scale)), cv2.LINE_AA)
    crop = cv2.resize(crop, (round(crop.shape[1] * scale), round(crop.shape[0] * scale)),
                      interpolation=cv2.INTER_AREA)
    ok, jpeg = cv2.imencode(".jpg", crop, [cv2.IMWRITE_JPEG_QUALITY, PHOTO_JPEG_QUALITY])
    return jpeg.tobytes() if ok else None


def make_preset(image_path, mask_path, directory=PRESET_DIRECTORY):
    values = macros.all_macros(image_path, mask_path)
    parameters = preset_parameters(values)
    frames = wavetable_frames(image_path, mask_path)
    name = object_name(image_path.stem)
    path = write_preset(
        directory / f"{name}.ospreset", parameters,
        frames=frames, wavetable_name=name, frame_index=0,
        photo_jpeg=object_photo(image_path, mask_path), object_name=name,
    )
    return path, values, parameters


def main():
    pairs = saved_pairs()
    if not pairs:
        print(f"No capture/mask pairs found in {CAPTURE_DIRECTORY} and {MASK_DIRECTORY}.")
        return

    for image_path, mask_path in pairs:
        try:
            path, values, parameters = make_preset(image_path, mask_path)
        except (ValueError, OSError) as error:
            print(f"{image_path.stem}: skipped ({error})")
            continue
        print(f"{path.name}: sharpness={values['sharpness']:.2f} "
              f"brightness={values['brightness']:.2f} body={values['body']:.2f} "
              f"movement={values['movement']:.2f} (asymmetry={values['asymmetry']:.2f} "
              f"texture={values['texture']:.2f} colorfulness={values['colorfulness']:.2f})")


if __name__ == "__main__":
    main()

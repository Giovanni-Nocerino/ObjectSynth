"""Capture, segment, and export an alpha-scanned Serum wavetable and an
ObjectSynth preset (.ospreset) with the wavetable and the macro mapping."""

from pathlib import Path

from acquisition import capture_image
from make_presets import make_preset, set_object_name
from recognition import segment_image
from to_wav import export_wavetable
from waveforom_extraction import extract_wavetable_features


def ask_object_name(capture_stem):
    """Optional human name for the preset; Enter keeps the capture's file name."""
    try:
        name = input("Nome dell'oggetto per il preset (Invio = nome della foto): ")
    except EOFError:
        name = ""
    return set_object_name(capture_stem, name)


def main():
    image_path = capture_image()
    if image_path is None:
        print("No photo saved. Pipeline stopped.")
        return

    mask_path = segment_image(image_path)
    if mask_path is None:
        print("No mask saved. Pipeline stopped.")
        return

    try:
        features_path = extract_wavetable_features(image_path, mask_path)
        export_wavetable(features_path)
    except (FileNotFoundError, ValueError, OSError) as error:
        print(f"Wavetable export failed: {error}")
        return

    # ObjectSynth preset: same wavetable + parameters from the four macros,
    # normalised with the stored calibration (macro_calibration.json).
    try:
        image_path, mask_path = Path(image_path), Path(mask_path)
        ask_object_name(image_path.stem)
        preset_path, values, _ = make_preset(image_path, mask_path)
    except (FileNotFoundError, ValueError, OSError) as error:
        print(f"Preset export failed: {error}")
        return

    print(f"ObjectSynth preset saved to: {preset_path}")
    print(f"Spigolosita {values['sharpness']:.2f} | Luminosita {values['brightness']:.2f} | "
          f"Corpo {values['body']:.2f} | Movimento {values['movement']:.2f}")
    print("Open it in ObjectSynth with OPEN PRESET.")


if __name__ == "__main__":
    main()

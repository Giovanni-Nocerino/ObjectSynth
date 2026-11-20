"""Build and export a multi-frame Serum wavetable from extracted features."""

import argparse
import json
import wave
from pathlib import Path

import numpy as np


CV_DIRECTORY = Path(__file__).resolve().parent
OUTPUT_DIRECTORY = CV_DIRECTORY / "waveforms"
BASIS_NAME = "luminance_profile_plus_radial_fft_v1"
HARMONIC_COUNT = 64
SAMPLES_PER_CYCLE = 2048
WAV_SAMPLE_RATE = 44100


def find_latest_features():
    """Return the newest feature file created by waveforom_extraction.py."""
    files = list(OUTPUT_DIRECTORY.glob("*_wavetable.json"))
    if not files:
        raise FileNotFoundError(
            f"No wavetable feature files found in {OUTPUT_DIRECTORY}. Run the pipeline first."
        )
    return max(files, key=lambda path: path.stat().st_mtime)


def choose_features_path():
    """Read an optional feature JSON path, or use the newest one."""
    parser = argparse.ArgumentParser(description="Export a multi-frame Serum wavetable.")
    parser.add_argument(
        "features",
        nargs="?",
        type=Path,
        help="Feature JSON. Defaults to the newest *_wavetable.json file.",
    )
    args = parser.parse_args()

    json_path = args.features if args.features else find_latest_features()
    if not json_path.is_file():
        raise FileNotFoundError(f"Feature file not found: {json_path}")
    return json_path


def normalize(values):
    """Scale non-negative values so their largest value is one."""
    values = np.asarray(values, dtype=np.float64)
    peak = np.max(values)
    if peak > 1e-8:
        return values / peak
    return np.zeros_like(values)


def build_frame(shape_coefficients, texture_amplitudes, alpha):
    """Mix shape and texture amplitudes and reconstruct one waveform cycle."""
    if len(shape_coefficients) != 2 * HARMONIC_COUNT:
        raise ValueError(f"Expected {2 * HARMONIC_COUNT} shape coefficients.")
    if len(texture_amplitudes) != HARMONIC_COUNT:
        raise ValueError(f"Expected {HARMONIC_COUNT} texture amplitudes.")

    sine_coefficients = np.asarray(shape_coefficients[0::2], dtype=np.float64)
    cosine_coefficients = np.asarray(shape_coefficients[1::2], dtype=np.float64)
    shape_amplitudes = np.hypot(sine_coefficients, cosine_coefficients)

    # A flat luminance profile still needs a smooth starting frame at alpha zero.
    if np.max(shape_amplitudes) < 1e-8:
        shape_amplitudes[0] = 1.0

    # Keep the profile's phase. A missing profile harmonic defaults to a sine phase.
    sine_direction = np.ones(HARMONIC_COUNT, dtype=np.float64)
    cosine_direction = np.zeros(HARMONIC_COUNT, dtype=np.float64)
    has_shape = shape_amplitudes > 1e-8
    sine_direction[has_shape] = sine_coefficients[has_shape] / shape_amplitudes[has_shape]
    cosine_direction[has_shape] = cosine_coefficients[has_shape] / shape_amplitudes[has_shape]

    shape_amplitudes = normalize(shape_amplitudes)
    texture_amplitudes = normalize(texture_amplitudes)
    amplitudes = (1.0 - alpha) * shape_amplitudes + alpha * texture_amplitudes

    # A flat image profile can have no non-DC harmonics; use a sine as a safe base.
    if np.max(amplitudes) < 1e-8:
        amplitudes[0] = 1.0

    phase = np.arange(SAMPLES_PER_CYCLE, dtype=np.float64) / SAMPLES_PER_CYCLE
    waveform = np.zeros(SAMPLES_PER_CYCLE, dtype=np.float64)

    for index in range(HARMONIC_COUNT):
        harmonic = index + 1
        waveform += amplitudes[index] * sine_direction[index] * np.sin(
            2 * np.pi * harmonic * phase
        )
        waveform += amplitudes[index] * cosine_direction[index] * np.cos(
            2 * np.pi * harmonic * phase
        )

    peak = np.max(np.abs(waveform))
    if peak < 1e-8:
        raise ValueError("The mixed coefficients produce a silent waveform.")
    return waveform * (0.95 / peak)


def build_wavetable(data):
    """Generate one audio cycle for every alpha value in the feature file."""
    shape_coefficients = data.get("shape_coefficients", [])
    texture_amplitudes = data.get("texture_amplitudes", [])
    alpha_values = data.get("alpha_values", [])

    if not alpha_values:
        raise ValueError("The feature file does not contain any alpha values.")
    if data.get("frame_count") != len(alpha_values):
        raise ValueError("Frame count does not match the alpha values.")
    if any(alpha < 0.0 or alpha > 1.0 for alpha in alpha_values):
        raise ValueError("Alpha values must be between 0 and 1.")

    frames = [
        build_frame(shape_coefficients, texture_amplitudes, alpha)
        for alpha in alpha_values
    ]
    return np.concatenate(frames)


def write_wav(path, samples):
    """Write mono 44.1 kHz 32-bit PCM audio."""
    pcm_samples = np.round(samples * (2**31 - 1)).astype(np.int32)

    with wave.open(str(path), "wb") as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(4)
        wav_file.setframerate(WAV_SAMPLE_RATE)
        wav_file.writeframes(pcm_samples.tobytes())


def export_wavetable(features_path):
    """Export feature data as a multi-frame WAV and Serum sidecar file."""
    json_path = Path(features_path)
    if not json_path.is_file():
        raise FileNotFoundError(f"Feature file not found: {json_path}")

    data = json.loads(json_path.read_text())
    if data.get("basis") != BASIS_NAME:
        raise ValueError(f"Unsupported wavetable feature format: {data.get('basis')}")
    if data.get("harmonic_count") != HARMONIC_COUNT:
        raise ValueError(f"Expected {HARMONIC_COUNT} harmonics in the feature file.")

    wavetable = build_wavetable(data)
    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)
    output_name = json_path.name.removesuffix("_wavetable.json") + "_serum_wavetable.wav"
    output_path = OUTPUT_DIRECTORY / output_name
    write_wav(output_path, wavetable)

    # Serum uses this to split the multi-cycle file into 2048-sample frames.
    output_path.with_suffix(".txt").write_text(f"[{SAMPLES_PER_CYCLE}]\n")

    frame_count = len(wavetable) // SAMPLES_PER_CYCLE
    print(f"Serum wavetable saved to: {output_path}")
    print(f"Frames: {frame_count} | samples per frame: {SAMPLES_PER_CYCLE}")
    print("Drag the multi-cycle WAV onto Serum's oscillator waveform display.")
    return output_path


def main():
    try:
        features_path = choose_features_path()
        export_wavetable(features_path)
    except (FileNotFoundError, ValueError, OSError) as error:
        print(error)


if __name__ == "__main__":
    main()

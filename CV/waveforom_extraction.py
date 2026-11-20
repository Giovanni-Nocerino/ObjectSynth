"""Extract shape and image-frequency data for a Serum wavetable."""

import argparse
import json
from pathlib import Path

import cv2
import numpy as np


CV_DIRECTORY = Path(__file__).resolve().parent
CAPTURE_DIRECTORY = CV_DIRECTORY / "captures"
MASK_DIRECTORY = CV_DIRECTORY / "masks"
OUTPUT_DIRECTORY = CV_DIRECTORY / "waveforms"
PROFILE_SAMPLE_COUNT = 512
FFT_IMAGE_SIZE = 256
HARMONIC_COUNT = 64
FRAME_COUNT = 32
BASIS_NAME = "luminance_profile_plus_radial_fft_v1"


def find_latest_capture():
    """Return the newest saved camera photo."""
    captures = list(CAPTURE_DIRECTORY.glob("capture_*.jpg"))
    if not captures:
        raise FileNotFoundError(f"No camera photos found in {CAPTURE_DIRECTORY}.")
    return max(captures, key=lambda path: path.stat().st_mtime)


def choose_input_paths():
    """Read image and mask paths, or find the mask for the newest photo."""
    parser = argparse.ArgumentParser(
        description="Extract wavetable data from an image and its object mask."
    )
    parser.add_argument("image", nargs="?", type=Path, help="Source camera image.")
    parser.add_argument("mask", nargs="?", type=Path, help="SAM object mask.")
    args = parser.parse_args()

    if args.image and args.mask:
        return args.image, args.mask
    if args.image or args.mask:
        raise ValueError("Provide both an image path and a mask path.")

    image_path = find_latest_capture()
    mask_path = MASK_DIRECTORY / f"{image_path.stem}_mask.png"
    if not mask_path.is_file():
        raise FileNotFoundError(f"Matching mask not found: {mask_path}")
    return image_path, mask_path


def read_masked_crop(image_path, mask_path):
    """Load the image and mask, then crop both to the object's bounding box."""
    image = cv2.imread(str(image_path))
    mask = cv2.imread(str(mask_path), cv2.IMREAD_GRAYSCALE)

    if image is None:
        raise ValueError(f"Could not open image: {image_path}")
    if mask is None:
        raise ValueError(f"Could not open mask: {mask_path}")
    if image.shape[:2] != mask.shape[:2]:
        raise ValueError("Image and mask dimensions do not match.")

    object_pixels = mask > 0
    y_coordinates, x_coordinates = np.where(object_pixels)
    if len(x_coordinates) == 0:
        raise ValueError("The mask is empty; SAM did not mark any object pixels.")

    left = int(x_coordinates.min())
    right = int(x_coordinates.max()) + 1
    top = int(y_coordinates.min())
    bottom = int(y_coordinates.max()) + 1

    cropped_image = image[top:bottom, left:right]
    cropped_mask = object_pixels[top:bottom, left:right]
    gray_image = cv2.cvtColor(cropped_image, cv2.COLOR_BGR2GRAY).astype(np.float64) / 255.0
    masked_luminance = np.where(cropped_mask, gray_image, 0.0)
    bounding_box = [left, top, right - left, bottom - top]
    return masked_luminance, bounding_box


def resample_profile(profile, sample_count):
    """Interpolate a 1D profile to a fixed number of samples."""
    if len(profile) < 2:
        raise ValueError("The cropped object is too narrow to create a profile.")

    source_positions = (np.arange(len(profile)) + 0.5) / len(profile)
    target_positions = (np.arange(sample_count) + 0.5) / sample_count
    return np.interp(target_positions, source_positions, profile)


def luminance_profile(masked_luminance):
    """Average luminance vertically, leaving one value per image column."""
    profile = masked_luminance.mean(axis=0, dtype=np.float64)
    profile = resample_profile(profile, PROFILE_SAMPLE_COUNT)
    profile -= profile.mean()

    peak = np.max(np.abs(profile))
    if peak > 1e-8:
        profile /= peak
    return profile


def profile_to_coefficients(profile):
    """Convert the 1D profile into sine and cosine coefficients."""
    spectrum = np.fft.rfft(profile) / len(profile)
    coefficients = []

    for harmonic in range(1, HARMONIC_COUNT + 1):
        sine_coefficient = -2 * spectrum[harmonic].imag
        cosine_coefficient = 2 * spectrum[harmonic].real
        coefficients.extend([float(sine_coefficient), float(cosine_coefficient)])

    return coefficients


def fit_image_to_square(image):
    """Resize an image to a fixed square while preserving its proportions."""
    height, width = image.shape
    usable_size = FFT_IMAGE_SIZE * 0.9  # Leave a small border around the masked object.
    scale = min(usable_size / width, usable_size / height)
    resized_width = max(1, round(width * scale))
    resized_height = max(1, round(height * scale))
    interpolation = cv2.INTER_AREA if scale < 1 else cv2.INTER_LINEAR
    resized = cv2.resize(image, (resized_width, resized_height), interpolation=interpolation)

    square = np.zeros((FFT_IMAGE_SIZE, FFT_IMAGE_SIZE), dtype=np.float64)
    left = (FFT_IMAGE_SIZE - resized_width) // 2
    top = (FFT_IMAGE_SIZE - resized_height) // 2
    square[top : top + resized_height, left : left + resized_width] = resized
    return square


def radial_fft_amplitudes(masked_luminance):
    """Summarize the 2D FFT by average energy in concentric frequency bands."""
    square_image = fit_image_to_square(masked_luminance)
    spectrum = np.fft.fftshift(np.fft.fft2(square_image))
    power = np.abs(spectrum) ** 2

    center = FFT_IMAGE_SIZE // 2
    y_coordinates, x_coordinates = np.ogrid[:FFT_IMAGE_SIZE, :FFT_IMAGE_SIZE]
    radius = np.sqrt((x_coordinates - center) ** 2 + (y_coordinates - center) ** 2)
    max_radius = FFT_IMAGE_SIZE / 2
    band_width = max_radius / HARMONIC_COUNT
    amplitudes = []

    for band in range(HARMONIC_COUNT):
        inner_radius = band * band_width
        outer_radius = (band + 1) * band_width
        in_band = (radius >= inner_radius) & (radius < outer_radius)
        if band == 0:
            in_band[center, center] = False  # Ignore the average brightness (DC).

        if np.any(in_band):
            amplitudes.append(float(np.sqrt(power[in_band].mean())))
        else:
            amplitudes.append(0.0)

    amplitudes = np.log1p(np.asarray(amplitudes, dtype=np.float64))
    peak = np.max(amplitudes)
    if peak > 1e-8:
        amplitudes /= peak
    return amplitudes.tolist()


def extract_wavetable_features(image_path, mask_path):
    """Save shape and texture features and return their JSON path."""
    image_path = Path(image_path)
    mask_path = Path(mask_path)
    if not image_path.is_file():
        raise FileNotFoundError(f"Image not found: {image_path}")
    if not mask_path.is_file():
        raise FileNotFoundError(f"Mask not found: {mask_path}")

    masked_luminance, bounding_box = read_masked_crop(image_path, mask_path)
    profile = luminance_profile(masked_luminance)
    shape_coefficients = profile_to_coefficients(profile)
    texture_amplitudes = radial_fft_amplitudes(masked_luminance)
    alpha_values = np.linspace(0.0, 1.0, FRAME_COUNT).tolist()

    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)
    output_name = image_path.stem + "_wavetable.json"
    output_path = OUTPUT_DIRECTORY / output_name

    result = {
        "basis": BASIS_NAME,
        "source_image": image_path.name,
        "source_mask": mask_path.name,
        "profile_method": "masked_luminance_mean_per_column",
        "texture_method": "mean_power_in_radial_2d_fft_bands",
        "crop_box_xywh": bounding_box,
        "profile_sample_count": PROFILE_SAMPLE_COUNT,
        "harmonic_count": HARMONIC_COUNT,
        "frame_count": FRAME_COUNT,
        "alpha_values": alpha_values,
        "shape_coefficients": shape_coefficients,
        "texture_amplitudes": texture_amplitudes,
        "luminance_profile": profile.tolist(),
    }
    output_path.write_text(json.dumps(result, indent=2) + "\n")

    print(f"Wavetable features saved to: {output_path}")
    print(f"Frames: {FRAME_COUNT} | harmonics: {HARMONIC_COUNT} | samples per frame: 2048")
    return output_path


def main():
    try:
        image_path, mask_path = choose_input_paths()
        extract_wavetable_features(image_path, mask_path)
    except (FileNotFoundError, ValueError, OSError) as error:
        print(error)


if __name__ == "__main__":
    main()

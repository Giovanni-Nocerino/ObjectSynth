"""Data for the "Wavetable conversion" tab of docs/explorer.html.

Runs the real wavetable code (waveforom_extraction.py) on one photo + mask and
adds small pictures of each step, so the page can animate them:
  - the cropped object as a coarse pixel grid (colour, luminance, mask);
  - the raw column means, the centred profile and its 64 harmonics;
  - the object fitted into the FFT square, its 2D power spectrum and the
    64 ring averages before and after the log compression.

macro_server.py sends this for "Try your own!" photos; CV/page_objects.py
embeds it in the page for every example object.
"""

import base64

import cv2
import numpy as np

import waveforom_extraction as extraction

GRID_COLUMNS = 64        # columns of the pixel grid that "falls" into the vector
SQUARE_CELLS = 64        # cells per side of the coloured FFT square
SPECTRUM_SIZE = 128      # the 256 x 256 spectrum is shown at this size (smaller page)


def to_base64(array):
    return base64.b64encode(np.clip(array, 0, 255).astype(np.uint8).tobytes()).decode()


def masked_colour_crop(image_path, mask_path, bounding_box):
    image = cv2.imread(str(image_path))
    mask = cv2.imread(str(mask_path), cv2.IMREAD_GRAYSCALE) > 0
    left, top, width, height = bounding_box
    crop = image[top:top + height, left:left + width].astype(np.float64)
    crop_mask = mask[top:top + height, left:left + width]
    crop[~crop_mask] = 0.0
    return cv2.cvtColor(crop.astype(np.float32), cv2.COLOR_BGR2RGB), crop_mask.astype(np.float32)


def shrink(image, width, height):
    return cv2.resize(image, (width, height), interpolation=cv2.INTER_AREA)


def ring_powers(square_image):
    """sqrt(mean power) per ring, before the log: the same bands as the extractor."""
    size = extraction.FFT_IMAGE_SIZE
    power = np.abs(np.fft.fftshift(np.fft.fft2(square_image))) ** 2
    centre = size // 2
    y, x = np.ogrid[:size, :size]
    radius = np.sqrt((x - centre) ** 2 + (y - centre) ** 2)
    band_width = (size / 2) / extraction.HARMONIC_COUNT
    values = []
    for band in range(extraction.HARMONIC_COUNT):
        in_band = (radius >= band * band_width) & (radius < (band + 1) * band_width)
        if band == 0:
            in_band[centre, centre] = False
        values.append(float(np.sqrt(power[in_band].mean())) if in_band.any() else 0.0)
    return power, np.asarray(values)


def story_data(image_path, mask_path, name):
    luminance, bounding_box = extraction.read_masked_crop(image_path, mask_path)
    colour, crop_mask = masked_colour_crop(image_path, mask_path, bounding_box)
    height, width = luminance.shape

    # Pixel grid: each cell is the mean of its block, so a grid column's mean
    # is (almost) the real column mean.
    grid_rows = int(np.clip(round(GRID_COLUMNS * height / width), 12, 96))
    grid_luminance = shrink(luminance.astype(np.float32), GRID_COLUMNS, grid_rows)
    grid_colour = shrink(colour, GRID_COLUMNS, grid_rows)
    grid_mask = shrink(crop_mask, GRID_COLUMNS, grid_rows)

    raw_profile = extraction.resample_profile(
        luminance.mean(axis=0), extraction.PROFILE_SAMPLE_COUNT)
    profile = extraction.luminance_profile(luminance)
    coefficients = extraction.profile_to_coefficients(profile)

    square = extraction.fit_image_to_square(luminance)
    colour_square = np.stack([extraction.fit_image_to_square(colour[..., c]) for c in range(3)], axis=-1)
    power, rings = ring_powers(square)
    spectrum = np.log1p(power)
    spectrum = (spectrum - spectrum.min()) / max(spectrum.max() - spectrum.min(), 1e-9)
    spectrum = shrink(spectrum.astype(np.float32), SPECTRUM_SIZE, SPECTRUM_SIZE)

    return {
        "name": name,
        "grid": {
            "columns": GRID_COLUMNS,
            "rows": grid_rows,
            "luminance": to_base64(grid_luminance * 255),
            "colour": to_base64(grid_colour),
            "mask": to_base64(grid_mask * 255),
        },
        "rawProfile": np.round(raw_profile, 4).tolist(),
        "profile": np.round(profile, 4).tolist(),
        "coefficients": np.round(coefficients, 6).tolist(),
        "square": {
            "cells": SQUARE_CELLS,
            "colour": to_base64(shrink(colour_square.astype(np.float32), SQUARE_CELLS, SQUARE_CELLS)),
            "luminance": to_base64(shrink(square.astype(np.float32), SQUARE_CELLS, SQUARE_CELLS) * 255),
        },
        "spectrum": {"size": SPECTRUM_SIZE, "pixels": to_base64(spectrum ** 1.4 * 255)},
        "ringPowers": np.round(rings / max(rings.max(), 1e-12), 6).tolist(),
        "texture": np.round(extraction.radial_fft_amplitudes(luminance), 6).tolist(),
    }


"""Macro features measured on the photo and its SAM object mask.

Spigolosita (sharpness): Harris corner detector (Harris & Stephens, 1988) in
its Shi-Tomasi form (Shi & Tomasi, 1994), applied to the object silhouette.

Luminosita (brightness): mean CIELAB L* inside the mask, after white-patch
exposure normalisation.

Corpo (body): linear size of the object in the frame plus its solidity.

Movimento (movement): three ingredients, each mapped to its own sound part:
asymmetry (mirror IoU on the principal axes), texture (Canny edge density,
Canny 1986) and colourfulness (Hasler & Suesstrunk, 2003).

Raw scores are mapped to 0..1 with a calibration (min/max over all saved
captures) stored in macro_calibration.json.
"""

import json
from pathlib import Path

import cv2
import numpy as np


CV_DIRECTORY = Path(__file__).resolve().parent
CALIBRATION_PATH = CV_DIRECTORY / "macro_calibration.json"

# The silhouette is rescaled so its longer side has this many pixels: corner
# responses then do not depend on how big the object is in the photo.
NORMALISED_SIZE = 512
CANVAS_PADDING = 48
FRAME_BORDER = 4  # pixels: the mask counts as touching the photo border

# Harris / Shi-Tomasi settings, in normalised-silhouette pixels.
BLUR_SIGMA = 3.0  # removes pixel staircase on the mask edge
BLOCK_SIZE = 9
HARRIS_K = 0.04
MAX_CORNERS = 40
MIN_CORNER_DISTANCE = 25
CONVEX_RADIUS = 14
CONVEX_MAX_FILL = 0.53  # below: the corner points outwards (a tip, not a notch)
BORDER_IGNORE = 12  # ignore corners created by the photo frame cutting the object

# Reference response for one "very sharp" corner. Responses are divided by it,
# so strengths are comparable across objects.
REFERENCE_RESPONSE = 2.5e-4
STRONG_CORNER = 0.25  # normalised strength above which a corner is counted
COUNT_FOR_FULL = 10   # this many strong corners saturate the count term
STRENGTH_WEIGHT = 0.6
COUNT_WEIGHT = 0.4


def _load_silhouette(mask_path):
    """Largest mask component, holes filled, rescaled and padded."""
    mask = cv2.imread(str(mask_path), cv2.IMREAD_GRAYSCALE)
    if mask is None:
        raise ValueError(f"Could not open mask: {mask_path}")
    mask = (mask > 127).astype(np.uint8)
    height, width = mask.shape

    count, labels, stats, _ = cv2.connectedComponentsWithStats(mask)
    if count < 2:
        raise ValueError(f"Mask is empty: {mask_path}")
    largest = 1 + int(np.argmax(stats[1:, cv2.CC_STAT_AREA]))
    mask = (labels == largest).astype(np.uint8)

    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    mask = np.zeros_like(mask)
    cv2.drawContours(mask, contours, -1, 1, thickness=-1)

    x, y, w, h = cv2.boundingRect(mask)
    touches = {
        "left": x <= FRAME_BORDER,
        "top": y <= FRAME_BORDER,
        "right": x + w >= width - FRAME_BORDER,
        "bottom": y + h >= height - FRAME_BORDER,
    }

    scale = NORMALISED_SIZE / max(w, h)
    size = (max(1, round(w * scale)), max(1, round(h * scale)))
    crop = cv2.resize(mask[y:y + h, x:x + w] * 255, size, interpolation=cv2.INTER_AREA)
    crop = cv2.copyMakeBorder(crop, CANVAS_PADDING, CANVAS_PADDING,
                              CANVAS_PADDING, CANVAS_PADDING, cv2.BORDER_CONSTANT, 0)
    return crop, touches


def _on_cut_border(x, y, shape, touches):
    height, width = shape
    limit = CANVAS_PADDING + BORDER_IGNORE
    return ((touches["left"] and x < limit) or (touches["top"] and y < limit)
            or (touches["right"] and x > width - limit)
            or (touches["bottom"] and y > height - limit))


def detect_sharp_corners(mask_path):
    """Return the silhouette and its outward-pointing corners (x, y, strength)."""
    silhouette, touches = _load_silhouette(mask_path)
    image = cv2.GaussianBlur(silhouette.astype(np.float32) / 255.0, (0, 0), BLUR_SIGMA)

    response = cv2.cornerHarris(image, BLOCK_SIZE, 3, HARRIS_K)
    candidates = cv2.goodFeaturesToTrack(
        image, maxCorners=MAX_CORNERS, qualityLevel=0.01,
        minDistance=MIN_CORNER_DISTANCE, blockSize=BLOCK_SIZE,
        useHarrisDetector=True, k=HARRIS_K,
    )
    if candidates is None:
        return silhouette, []

    corners = []
    disk = np.zeros_like(image)
    for x, y in candidates[:, 0, :]:
        if _on_cut_border(x, y, image.shape, touches):
            continue
        value = float(response[int(y), int(x)])
        if value <= 0.0:
            continue
        disk[:] = 0
        cv2.circle(disk, (int(x), int(y)), CONVEX_RADIUS, 1.0, -1)
        fill = float((image * disk).sum() / disk.sum())
        if fill < CONVEX_MAX_FILL:
            corners.append((float(x), float(y), value / REFERENCE_RESPONSE))
    return silhouette, corners


def sharpness_raw(mask_path):
    """Uncalibrated sharpness: strength of the sharpest tips plus how many there are."""
    _, corners = detect_sharp_corners(mask_path)
    strengths = np.sort([s for _, _, s in corners])[::-1]
    strength = float(np.clip(strengths[:3].mean(), 0.0, 1.0)) if len(strengths) else 0.0
    count = int(np.sum(strengths > STRONG_CORNER))
    return STRENGTH_WEIGHT * strength + COUNT_WEIGHT * min(count / COUNT_FOR_FULL, 1.0)


WHITE_PATCH_PERCENTILE = 99  # brightest scene pixels taken as the white reference


def brightness_raw(image_path, mask_path):
    """Mean CIELAB L* (0..100) of the object, after white-patch exposure normalisation.

    White patch (Land's Retinex idea): the brightest part of the whole photo is
    assumed to be white, so the same object photographed darker or brighter
    gives a similar value. L* follows perceived lightness, unlike raw RGB.
    """
    image = cv2.imread(str(image_path))
    mask = cv2.imread(str(mask_path), cv2.IMREAD_GRAYSCALE)
    if image is None:
        raise ValueError(f"Could not open image: {image_path}")
    if mask is None:
        raise ValueError(f"Could not open mask: {mask_path}")
    mask = cv2.resize(mask, (image.shape[1], image.shape[0]),
                      interpolation=cv2.INTER_NEAREST) > 127
    if not mask.any():
        raise ValueError(f"Mask is empty: {mask_path}")

    image = image.astype(np.float32) / 255.0
    white = max(float(np.percentile(image.max(axis=2), WHITE_PATCH_PERCENTILE)), 1e-3)
    balanced = np.clip(image / white, 0.0, 1.0)
    lightness = cv2.cvtColor(balanced, cv2.COLOR_BGR2LAB)[..., 0]
    return float(lightness[mask].mean())


BODY_SIZE_WEIGHT = 0.7
BODY_SOLIDITY_WEIGHT = 0.3


def body_raw(mask_path):
    """How big and "full" the object looks in the photo.

    Size: sqrt of the mask's share of the frame, i.e. its linear size (it
    changes with the distance from the camera, which is accepted: the same
    object gives different presets near and far). Solidity: area / convex hull
    area, 1 for full shapes, lower for thin or open ones (keys, headphones).
    """
    mask = cv2.imread(str(mask_path), cv2.IMREAD_GRAYSCALE)
    if mask is None:
        raise ValueError(f"Could not open mask: {mask_path}")
    mask = (mask > 127).astype(np.uint8)
    count, labels, stats, _ = cv2.connectedComponentsWithStats(mask)
    if count < 2:
        raise ValueError(f"Mask is empty: {mask_path}")
    mask = (labels == 1 + int(np.argmax(stats[1:, cv2.CC_STAT_AREA]))).astype(np.uint8)

    contour = max(cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)[0],
                  key=cv2.contourArea)
    area = cv2.contourArea(contour)
    hull_area = cv2.contourArea(cv2.convexHull(contour))
    linear_size = float(np.sqrt(area / mask.size))
    solidity = float(area / hull_area) if hull_area > 0 else 0.0
    return BODY_SIZE_WEIGHT * linear_size + BODY_SOLIDITY_WEIGHT * solidity


def _largest_component(mask):
    count, labels, stats, _ = cv2.connectedComponentsWithStats(mask)
    if count < 2:
        raise ValueError("Mask is empty.")
    return (labels == 1 + int(np.argmax(stats[1:, cv2.CC_STAT_AREA]))).astype(np.uint8)


def _read_pair(image_path, mask_path):
    """Photo and its largest mask component, at the photo's resolution."""
    image = cv2.imread(str(image_path))
    mask = cv2.imread(str(mask_path), cv2.IMREAD_GRAYSCALE)
    if image is None:
        raise ValueError(f"Could not open image: {image_path}")
    if mask is None:
        raise ValueError(f"Could not open mask: {mask_path}")
    mask = cv2.resize(mask, (image.shape[1], image.shape[0]),
                      interpolation=cv2.INTER_NEAREST)
    return image, _largest_component((mask > 127).astype(np.uint8))


def asymmetry_raw(mask_path):
    """0 for a mirror-symmetric silhouette, higher for lopsided ones.

    The silhouette is aligned to its principal axes (image moments) and
    mirrored across each of them; the score is 1 - IoU with its mirror image,
    taking the more symmetric of the two axes.
    """
    mask = cv2.imread(str(mask_path), cv2.IMREAD_GRAYSCALE)
    if mask is None:
        raise ValueError(f"Could not open mask: {mask_path}")
    mask = _largest_component((mask > 127).astype(np.uint8))

    moments = cv2.moments(mask, binaryImage=True)
    cx = moments["m10"] / moments["m00"]
    cy = moments["m01"] / moments["m00"]
    angle = 0.5 * np.degrees(np.arctan2(2 * moments["mu11"], moments["mu20"] - moments["mu02"]))
    size = (mask.shape[1], mask.shape[0])
    mirror = np.float32([[-1, 0, 2 * cx], [0, 1, 0]])

    best = 1.0
    for axis in (0.0, 90.0):
        rotation = cv2.getRotationMatrix2D((cx, cy), angle + axis, 1.0)
        aligned = cv2.warpAffine(mask, rotation, size, flags=cv2.INTER_NEAREST)
        mirrored = cv2.warpAffine(aligned, mirror, size, flags=cv2.INTER_NEAREST)
        union = np.logical_or(aligned, mirrored).sum()
        overlap = np.logical_and(aligned, mirrored).sum()
        best = min(best, 1.0 - overlap / max(union, 1))
    return float(best)


TEXTURE_EDGE_MARGIN = 15  # px eroded from the mask so the outline is not counted


def texture_raw(image_path, mask_path):
    """Share of Canny edge pixels inside the object: lines, print, patterns."""
    image, mask = _read_pair(image_path, mask_path)
    inner = cv2.erode(mask, np.ones((TEXTURE_EDGE_MARGIN, TEXTURE_EDGE_MARGIN), np.uint8)) > 0
    if not inner.any():
        return 0.0
    gray = cv2.GaussianBlur(cv2.cvtColor(image, cv2.COLOR_BGR2GRAY), (0, 0), 1.5)
    edges = cv2.Canny(gray, 50, 150) > 0
    return float(edges[inner].mean())


def colorfulness_raw(image_path, mask_path):
    """Hasler & Suesstrunk (2003) colourfulness of the object's pixels."""
    image, mask = _read_pair(image_path, mask_path)
    blue, green, red = (image[..., i][mask > 0].astype(np.float64) for i in range(3))
    rg = red - green
    yb = 0.5 * (red + green) - blue
    return float(np.hypot(rg.std(), yb.std()) + 0.3 * np.hypot(rg.mean(), yb.mean()))


def load_calibration():
    if CALIBRATION_PATH.is_file():
        return json.loads(CALIBRATION_PATH.read_text())
    return {}


def save_calibration(calibration):
    CALIBRATION_PATH.write_text(json.dumps(calibration, indent=2) + "\n")


def calibrate(name, raw_values):
    """Store the min/max of a raw macro over a set of objects."""
    calibration = load_calibration()
    calibration[name] = {"min": float(min(raw_values)), "max": float(max(raw_values)),
                         "samples": len(raw_values)}
    save_calibration(calibration)
    return calibration[name]


def normalise(name, raw):
    """Map a raw macro value to 0..1 with the stored calibration."""
    entry = load_calibration().get(name)
    if not entry or entry["max"] - entry["min"] < 1e-9:
        return float(np.clip(raw, 0.0, 1.0))
    return float(np.clip((raw - entry["min"]) / (entry["max"] - entry["min"]), 0.0, 1.0))


def sharpness(mask_path):
    """Spigolosita in 0..1."""
    return normalise("sharpness", sharpness_raw(mask_path))


def brightness(image_path, mask_path):
    """Luminosita in 0..1."""
    return normalise("brightness", brightness_raw(image_path, mask_path))


def body(mask_path):
    """Corpo in 0..1."""
    return normalise("body", body_raw(mask_path))


def movement(image_path, mask_path):
    """Movimento: three ingredients in 0..1, each driving its own part of the
    sound, plus their mean (used only to place the object in the macro space)."""
    parts = {
        "asymmetry": normalise("asymmetry", asymmetry_raw(mask_path)),
        "texture": normalise("texture", texture_raw(image_path, mask_path)),
        "colorfulness": normalise("colorfulness", colorfulness_raw(image_path, mask_path)),
    }
    parts["movement"] = float(np.mean(list(parts.values())))
    return parts


def raw_values(image_path, mask_path):
    """Every uncalibrated measurement, keyed like the calibration entries."""
    return {
        "sharpness": sharpness_raw(mask_path),
        "brightness": brightness_raw(image_path, mask_path),
        "body": body_raw(mask_path),
        "asymmetry": asymmetry_raw(mask_path),
        "texture": texture_raw(image_path, mask_path),
        "colorfulness": colorfulness_raw(image_path, mask_path),
    }


def all_macros(image_path, mask_path):
    """All macros (and Movimento's ingredients) in 0..1."""
    values = {
        "sharpness": sharpness(mask_path),
        "brightness": brightness(image_path, mask_path),
        "body": body(mask_path),
    }
    values.update(movement(image_path, mask_path))
    return values

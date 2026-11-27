"""Macro features measured on the photo and its SAM object mask.

Sharpness: Harris corner response (Harris & Stephens, 1988) on
the object silhouette; candidate corners are picked with OpenCV's
goodFeaturesToTrack (quality threshold, minimum distance, non-max suppression)
but scored with Harris, not with Shi-Tomasi's min eigenvalue.

Brightness: mean CIELAB L* inside the mask, after white-patch
exposure normalisation.

Body: linear size of the object in the frame.

Movement: three ingredients, each mapped to its own sound part:
asymmetry (mirror IoU on the principal axes), texture (Canny edge density,
Canny 1986) and colourfulness (mean CIELAB chroma C*).

Raw scores are mapped to 0..1 with a fixed range per macro (MACRO_RANGES):
the raw value at `min` gives 0, at `max` gives 1, and values outside are
clipped, so one odd object does not stretch the scale.
"""

from pathlib import Path

import cv2
import numpy as np


CV_DIRECTORY = Path(__file__).resolve().parent

# Raw value giving 0 and raw value giving 1, per macro.
MACRO_RANGES = {
    "sharpness": (0.0337791204219684, 1.118572393668816),        # Harris tip score
    "brightness": (18.921255111694336, 69.69278717041013),       # mean L* (0..100)
    "body": (0.08, 0.65),                                        # object size / frame size
    "asymmetry": (0.108819174659801, 0.8327812406284156),        # 1 - mirror IoU
    "texture": (0.0025666777586676555, 0.09157268286631141),     # Canny edge density
    "colorfulness": (3.511051225662232, 39.082034301757766),     # mean chroma C*
}

# The silhouette is rescaled so its longer side has this many pixels: corner
# responses then do not depend on how big the object is in the photo.
NORMALISED_SIZE = 512
CANVAS_PADDING = 48
FRAME_BORDER = 4  # pixels: the mask counts as touching the photo border

# Harris settings, in normalised-silhouette pixels.
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
COUNT_SCALE = 10      # strong corners counted in units of this many (no ceiling)
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
    """Raw sharpness: strength of the sharpest tips plus how many there are.

    No ceiling on either term: very spiky objects keep scoring higher, and
    MACRO_RANGES decides where 0 and 1 fall.
    """
    _, corners = detect_sharp_corners(mask_path)
    strengths = np.sort([s for _, _, s in corners])[::-1]
    strength = float(strengths[:3].mean()) if len(strengths) else 0.0
    count = int(np.sum(strengths > STRONG_CORNER))
    return STRENGTH_WEIGHT * strength + COUNT_WEIGHT * count / COUNT_SCALE


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


def body_raw(mask_path):
    """How big the object looks in the photo: sqrt of the mask's share of the
    frame, i.e. its linear size. It changes with the distance from the camera,
    which is accepted: the same object gives different presets near and far.
    """
    mask = cv2.imread(str(mask_path), cv2.IMREAD_GRAYSCALE)
    if mask is None:
        raise ValueError(f"Could not open mask: {mask_path}")
    mask = _largest_component((mask > 127).astype(np.uint8))
    return float(np.sqrt(mask.sum() / mask.size))


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
    """Mean CIELAB chroma C* = sqrt(a*^2 + b*^2) of the object's pixels.

    Same perceptual space and white-patch normalisation as the brightness, so
    a grey object gives ~0 whatever its lightness.
    """
    image, mask = _read_pair(image_path, mask_path)
    image = image.astype(np.float32) / 255.0
    white = max(float(np.percentile(image.max(axis=2), WHITE_PATCH_PERCENTILE)), 1e-3)
    lab = cv2.cvtColor(np.clip(image / white, 0.0, 1.0), cv2.COLOR_BGR2LAB)
    chroma = np.hypot(lab[..., 1], lab[..., 2])
    return float(chroma[mask > 0].mean())


def normalise(name, raw):
    """Map a raw macro value to 0..1 with its range in MACRO_RANGES."""
    low, high = MACRO_RANGES[name]
    return float(np.clip((raw - low) / (high - low), 0.0, 1.0))


def sharpness(mask_path):
    """Sharpness in 0..1."""
    return normalise("sharpness", sharpness_raw(mask_path))


def brightness(image_path, mask_path):
    """Brightness in 0..1."""
    return normalise("brightness", brightness_raw(image_path, mask_path))


def body(mask_path):
    """Body in 0..1."""
    return normalise("body", body_raw(mask_path))


def movement(image_path, mask_path):
    """Movement: three ingredients in 0..1, each driving its own part of the
    sound, plus their mean (used only to place the object in the macro space)."""
    parts = {
        "asymmetry": normalise("asymmetry", asymmetry_raw(mask_path)),
        "texture": normalise("texture", texture_raw(image_path, mask_path)),
        "colorfulness": normalise("colorfulness", colorfulness_raw(image_path, mask_path)),
    }
    parts["movement"] = float(np.mean(list(parts.values())))
    return parts


def all_macros(image_path, mask_path):
    """All macros (and Movement's ingredients) in 0..1."""
    values = {
        "sharpness": sharpness(mask_path),
        "brightness": brightness(image_path, mask_path),
        "body": body(mask_path),
    }
    values.update(movement(image_path, mask_path))
    return values

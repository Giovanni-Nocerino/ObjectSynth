"""Calibrate every macro on DIS5K.

DIS5K (Qin et al., "Highly Accurate Dichotomous Image Segmentation", ECCV 2022):
5470 real photos of objects from 225 categories with hand-made, highly accurate
masks. Download DIS5K.zip (5.3 GB) from the Google Drive link in
https://github.com/xuebinqin/DIS and unzip it into CV/datasets/dis5k-download/
(CV/datasets/ is git-ignored), so the photos are in
CV/datasets/dis5k-download/DIS5K/DIS-*/im/ and the masks in .../gt/.

DIS5K favours intricate objects (furniture, buildings, vehicles, insects), so
the shape macros (SHAPE_MACROS: sharpness, asymmetry) are calibrated only on
the everyday, hand-sized objects (EVERYDAY_GROUPS, EVERYDAY_TOOLS); the other
macros on a random sample of the whole dataset. The category is in each file
name: <n>#<Group>#<n>#<Subcategory>#<id>.jpg.

Steps:
  1. a random sample of SAMPLE_SIZE photos (fixed seed, all splits) plus every
     everyday-object photo;
  2. photo and mask rescaled so the long side matches our captures (1920 px):
     texture (edge density) depends on the resolution;
  3. masks covering almost nothing or the whole frame are skipped;
  4. raw macros on every photo + mask, cached in datasets/dis5k-raw.json;
  5. 5th..95th percentile of each macro stored in macro_calibration.json.

Usage:
  python calibrate_dis5k.py
"""

import json
import random

import cv2
import numpy as np

import macros


DATASET_DIRECTORY = macros.CV_DIRECTORY / "datasets"
DIS5K_DIRECTORY = DATASET_DIRECTORY / "dis5k-download" / "DIS5K"
RESIZED_DIRECTORY = DATASET_DIRECTORY / "dis5k-1920"
RAW_VALUES_PATH = DATASET_DIRECTORY / "dis5k-raw.json"

SAMPLE_SIZE = 1000
SEED = 0
LONG_SIDE = 1920  # our camera captures are 1920x1080
MIN_AREA_FRACTION, MAX_AREA_FRACTION = 0.02, 0.95

SHAPE_MACROS = ("sharpness", "asymmetry")
EVERYDAY_GROUPS = {"Kitchenware", "Accessories", "Electronics"}
EVERYDAY_EXCLUDED = {"Antenna"}  # a structure, not a hand-sized object
EVERYDAY_TOOLS = {
    "Key", "Clip", "Scissor", "Clock", "Stationary", "Scale", "Tapeline", "Comb",
    "Hammer", "Button", "Needle", "TobaccoPipe", "Stethoscope",
}


def category(path):
    """(group, subcategory) from a DIS5K file name."""
    parts = path.name.split("#")
    return (parts[1], parts[3]) if len(parts) >= 5 else ("", "")


def is_everyday(path):
    group, subcategory = category(path)
    if group == "Tool":
        return subcategory in EVERYDAY_TOOLS
    return group in EVERYDAY_GROUPS and subcategory not in EVERYDAY_EXCLUDED


def stem_of(image_path):
    return f"{image_path.parent.parent.name}_{image_path.stem}"


def all_pairs():
    pairs = []
    for image_path in sorted(DIS5K_DIRECTORY.glob("DIS-*/im/*.jpg")):
        mask_path = image_path.parent.parent / "gt" / (image_path.stem + ".png")
        if mask_path.is_file():
            pairs.append((image_path, mask_path))
    return pairs


def sample_pairs(pairs):
    shuffled = list(pairs)
    random.Random(SEED).shuffle(shuffled)
    return shuffled[:SAMPLE_SIZE]


def resized_pair(image_path, mask_path):
    """Photo and mask at our captures' resolution, or None for unusable masks."""
    image = cv2.imread(str(image_path))
    mask = cv2.imread(str(mask_path), cv2.IMREAD_GRAYSCALE)
    if image is None or mask is None:
        return None
    scale = LONG_SIDE / max(image.shape[:2])
    size = (round(image.shape[1] * scale), round(image.shape[0] * scale))
    image = cv2.resize(image, size, interpolation=cv2.INTER_AREA)
    mask = cv2.resize(mask, size, interpolation=cv2.INTER_NEAREST)
    if not MIN_AREA_FRACTION <= (mask > 127).mean() <= MAX_AREA_FRACTION:
        return None

    RESIZED_DIRECTORY.mkdir(parents=True, exist_ok=True)
    stem = stem_of(image_path)
    out_image = RESIZED_DIRECTORY / f"{stem}.jpg"
    out_mask = RESIZED_DIRECTORY / f"{stem}_mask.png"
    cv2.imwrite(str(out_image), image, [cv2.IMWRITE_JPEG_QUALITY, 95])
    cv2.imwrite(str(out_mask), mask)
    return stem, out_image, out_mask


def measure(pairs, cache):
    """Raw macros of the usable pairs, reusing (and updating) the cache."""
    raw = {}
    for index, (image_path, mask_path) in enumerate(pairs, 1):
        stem = stem_of(image_path)
        if stem in cache:
            if cache[stem] is not None:
                raw[stem] = cache[stem]
            continue
        resized = resized_pair(image_path, mask_path)
        values = None
        if resized is not None:
            try:
                values = macros.raw_values(resized[1], resized[2])
            except ValueError:
                pass
        cache[stem] = values
        if values is not None:
            raw[stem] = values
        if index % 100 == 0:
            print(f"{index}/{len(pairs)} done", flush=True)
    RAW_VALUES_PATH.write_text(json.dumps(cache, indent=1) + "\n")
    return raw


def calibrate(names, raw, label):
    for name in names:
        entry = macros.calibrate(name, [r[name] for r in raw.values()], label)
        print(f"{name} calibrated on {entry['samples']} {label} photos "
              f"(raw p5 {entry['min']:.3f} .. p95 {entry['max']:.3f})")


def main():
    pairs = all_pairs()
    if not pairs:
        print(f"DIS5K not found in {DIS5K_DIRECTORY}. See this file's docstring.")
        return
    cache = json.loads(RAW_VALUES_PATH.read_text()) if RAW_VALUES_PATH.is_file() else {}

    general = measure(sample_pairs(pairs), cache)
    everyday = measure([pair for pair in pairs if is_everyday(pair[0])], cache)

    names = next(iter(general.values())).keys()
    calibrate([n for n in names if n not in SHAPE_MACROS], general, "dis5k")
    calibrate(SHAPE_MACROS, everyday, "dis5k-everyday")


if __name__ == "__main__":
    main()

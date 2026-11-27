"""Embed the example objects' photos in docs/macro-map.html (the Polaroid).

For each object of the page's OBJECTS list, finds the dataset photo its
preset was made from (same pick as dataset-presets/make_dataset_presets.py),
checks that its macros match the page, and stores a small thumbnail made
like the presets' own (CV/make_presets.object_photo).

Run from the repository root:
  .venv/bin/python CV/page_photos.py
"""

import base64
import json
import random
import re
import sys
from pathlib import Path

import cv2
import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "dataset-presets"))

import macros  # noqa: E402
from make_dataset_presets import DEFAULT_PHOTOS, SEED, photo_pairs, tag_name  # noqa: E402
from make_presets import object_photo  # noqa: E402

PAGE = ROOT / "docs" / "macro-map.html"
THUMB_SIDE = 260
THUMB_QUALITY = 74


def dataset_pick():
    """name -> (photo, mask) in the same order make_dataset_presets.py tries them."""
    by_name = {}
    for photo, mask in photo_pairs(DEFAULT_PHOTOS):
        by_name.setdefault(tag_name(photo.stem), []).append((photo, mask))
    rng = random.Random(SEED)
    names = sorted(by_name)
    rng.shuffle(names)
    picks = {}
    for name in names:
        candidates = by_name[name]
        rng.shuffle(candidates)
        picks[name] = candidates
    return picks


def thumbnail(photo, mask):
    jpeg = object_photo(photo, mask)
    image = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR)
    scale = THUMB_SIDE / max(image.shape[:2])
    image = cv2.resize(image, (round(image.shape[1] * scale), round(image.shape[0] * scale)),
                       interpolation=cv2.INTER_AREA)
    ok, data = cv2.imencode(".jpg", image, [cv2.IMWRITE_JPEG_QUALITY, THUMB_QUALITY])
    return "data:image/jpeg;base64," + base64.b64encode(data.tobytes()).decode()


def main():
    html = PAGE.read_text()
    objects = [json.loads(line.strip().rstrip(","))
               for line in re.search(r"const OBJECTS = \[\n(.*?)\n  \];", html, re.S).group(1).splitlines()]
    picks = dataset_pick()
    # objects that are not dataset photos keep the photo already in the page
    photos = json.loads(re.search(r'<script id="objectPhotos" type="application/json">(.*?)</script>', html, re.S).group(1))
    for entry in objects:
        name = entry["name"]
        for photo, mask in picks.get(name, []):
            try:
                values = macros.all_macros(photo, mask)
            except (ValueError, OSError):
                continue
            off = max(abs(values[key] - entry[key]) for key in values if key in entry and key != "name")
            if off > 0.002:
                print(f"{name}: {photo.name} does not match the page (off by {off:.3f})")
            photos[name] = thumbnail(photo, mask)
            break
        else:
            print(f"{name}: no dataset photo, " + ("kept the page's photo" if name in photos else "no photo"))
    block = f'<script id="objectPhotos" type="application/json">{json.dumps(photos, separators=(",", ":"))}</script>'
    html, count = re.subn(r'<script id="objectPhotos" type="application/json">.*?</script>',
                          lambda _: block, html, flags=re.S)
    if count != 1:
        sys.exit(f"No objectPhotos block in {PAGE}")
    PAGE.write_text(html)
    print(f"{len(photos)} photos embedded in {PAGE}")


if __name__ == "__main__":
    main()

"""Make ObjectSynth presets from photos of a segmentation dataset.

Takes photos that already have an object mask (no SAM, no camera) and runs
them through the same preset builder as the camera pipeline
(CV/make_presets.py), writing <name>.ospreset next to this script.

Expected input: a folder of photo + mask pairs, `<stem>.jpg` and
`<stem>_mask.png`, where the stem carries tags separated by '#', e.g.
`DIS-TR_1#Accessories#1#Bag#1234_o.jpg`. The 4th field (`Bag`) is the object's
name. A photo without tags is named after its file.

One photo per object name (so the presets are all different objects), names
picked at random with a fixed seed, so the same command gives the same set.

Usage (from the repository root):
  .venv/bin/python dataset-presets/make_dataset_presets.py [photo folder] [--count 100]
"""

import argparse
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "CV"))

import macros  # noqa: E402
from make_presets import object_photo, wavetable_frames  # noqa: E402
from macro_mapping import preset_parameters  # noqa: E402
from ospreset import write_preset  # noqa: E402

DEFAULT_PHOTOS = HERE.parent / "CV" / "datasets" / "dis5k-1920"
SEED = 7
# Tags renamed to a plainer word, and objects left out. Both are applied after the
# random pick, so the other presets stay the same photos.
RENAME = {"FerrisWheel": "ObservationWheel"}
SKIP = {"UnevenBars"}


def tag_name(stem):
    """Object name from the photo's tags, or the file name without tags."""
    fields = stem.split("#")
    name = fields[3] if len(fields) > 4 else stem
    return " ".join(name.replace("_", " ").replace("-", " ").split())


def photo_pairs(folder):
    """(photo, mask) for every photo that has a mask."""
    pairs = []
    for mask in sorted(folder.glob("*_mask.png")):
        photo = mask.with_name(mask.name.removesuffix("_mask.png") + ".jpg")
        if photo.is_file():
            pairs.append((photo, mask))
    return pairs


def make_preset(photo, mask, name):
    values = macros.all_macros(photo, mask)
    frames = wavetable_frames(photo, mask)
    write_preset(
        HERE / f"{name}.ospreset", preset_parameters(values),
        frames=frames, wavetable_name=name, frame_index=0,
        photo_jpeg=object_photo(photo, mask), object_name=name,
    )
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("photos", nargs="?", type=Path, default=DEFAULT_PHOTOS)
    parser.add_argument("--count", type=int, default=100)
    args = parser.parse_args()

    by_name = {}
    for photo, mask in photo_pairs(args.photos):
        by_name.setdefault(tag_name(photo.stem), []).append((photo, mask))
    if not by_name:
        sys.exit(f"No photo + mask pairs in {args.photos}")

    rng = random.Random(SEED)
    names = sorted(by_name)
    rng.shuffle(names)

    made = 0
    for name in names:
        if made == args.count:
            break
        candidates = by_name[name]
        rng.shuffle(candidates)
        if name in SKIP:   # counted, so the set of other objects does not change
            made += 1
            continue
        for photo, mask in candidates:  # next photo of the same object if one fails
            try:
                values = make_preset(photo, mask, RENAME.get(name, name))
            except (ValueError, OSError) as error:
                print(f"{name}: {photo.name} skipped ({error})")
                continue
            made += 1
            print(f"{made:3d} {name}: sharpness={values['sharpness']:.2f} "
                  f"brightness={values['brightness']:.2f} body={values['body']:.2f} "
                  f"movement={values['movement']:.2f}")
            break
    print(f"{made} presets in {HERE}")


if __name__ == "__main__":
    main()

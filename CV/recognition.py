"""Prompt SAM to segment an object from a saved camera image."""

import argparse
from pathlib import Path

import cv2
from ultralytics import SAM


CV_DIRECTORY = Path(__file__).resolve().parent
CAPTURE_DIRECTORY = CV_DIRECTORY / "captures"
MASK_DIRECTORY = CV_DIRECTORY / "masks"
MODEL_PATH = CV_DIRECTORY / "models" / "sam2.1_b.pt"
WINDOW_NAME = "PlayAnyThing - Select object"
MAX_WINDOW_WIDTH = 1100
MAX_WINDOW_HEIGHT = 750


def find_latest_capture():
    """Return the newest saved camera photo."""
    captures = list(CAPTURE_DIRECTORY.glob("capture_*.jpg"))

    if not captures:
        raise FileNotFoundError(
            f"No camera photos found in {CAPTURE_DIRECTORY}. Run acquisition.py first."
        )

    return max(captures, key=lambda path: path.stat().st_mtime)


def choose_image_path():
    """Use an image path from the command line or the newest camera photo."""
    parser = argparse.ArgumentParser(description="Segment an object with SAM.")
    parser.add_argument(
        "image",
        nargs="?",
        type=Path,
        help="Image to segment. Defaults to the newest photo in CV/captures.",
    )
    args = parser.parse_args()

    image_path = args.image if args.image else find_latest_capture()
    if not image_path.is_file():
        raise FileNotFoundError(f"Image not found: {image_path}")
    return image_path


def draw_preview(image, scale_x, scale_y, points, labels, mask=None):
    """Draw prompts and, when available, the selected SAM mask."""
    preview = image.copy()

    if mask is not None:
        overlay = image.copy()
        overlay[mask > 0] = (0, 220, 0)
        preview = cv2.addWeighted(image, 0.65, overlay, 0.35, 0)

    height, width = image.shape[:2]
    display_size = (round(width / scale_x), round(height / scale_y))
    preview = cv2.resize(preview, display_size)

    if mask is None:
        instructions = "Left click: object (+) | Right click: hand/background (-) | Enter: segment | R: clear | Q: quit"
    else:
        instructions = "Click more prompts to refine | Enter: run again | S: save | R: clear | Q: quit"

    cv2.putText(
        preview,
        instructions,
        (12, 28),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.55,
        (255, 255, 255),
        2,
    )

    for point, label in zip(points, labels):
        x = round(point[0] / scale_x)
        y = round(point[1] / scale_y)
        color = (0, 255, 0) if label == 1 else (0, 0, 255)
        symbol = "+" if label == 1 else "-"
        cv2.circle(preview, (x, y), 8, color, -1)
        cv2.putText(
            preview,
            symbol,
            (x + 10, y - 10),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            color,
            2,
        )

    return preview


def handle_mouse_click(event, x, y, flags, state):
    """Store a positive object point or a negative hand/background point."""
    if event == cv2.EVENT_LBUTTONDOWN:
        label = 1
    elif event == cv2.EVENT_RBUTTONDOWN:
        label = 0
    else:
        return

    image_x = round(x * state["scale_x"])
    image_y = round(y * state["scale_y"])
    state["points"].append([image_x, image_y])
    state["labels"].append(label)
    state["new_prompt"] = True


def run_sam(image, image_path):
    """Run SAM with the points selected by the user."""
    state = {"points": [], "labels": [], "new_prompt": False}

    image_height, image_width = image.shape[:2]
    scale = min(
        1.0,
        MAX_WINDOW_WIDTH / image_width,
        MAX_WINDOW_HEIGHT / image_height,
    )
    display_width = round(image_width * scale)
    display_height = round(image_height * scale)
    state["scale_x"] = image_width / display_width
    state["scale_y"] = image_height / display_height

    cv2.namedWindow(WINDOW_NAME, cv2.WINDOW_AUTOSIZE)
    cv2.setMouseCallback(WINDOW_NAME, handle_mouse_click, state)

    mask = None
    model = None

    try:
        while True:
            if state["new_prompt"]:
                mask = None
                state["new_prompt"] = False

            preview = draw_preview(
                image,
                state["scale_x"],
                state["scale_y"],
                state["points"],
                state["labels"],
                mask,
            )
            cv2.imshow(WINDOW_NAME, preview)
            key = cv2.waitKey(20) & 0xFF

            if key in (ord("q"), 27):
                return None
            if key == ord("r"):
                state["points"].clear()
                state["labels"].clear()
                state["new_prompt"] = False
                mask = None
                print("Prompts cleared. Click the object and, if needed, the hand again.")
            elif key in (10, 13) and mask is None:
                if 1 not in state["labels"]:
                    print("Add at least one left click on the object first.")
                    continue

                if model is None:
                    print(f"Loading SAM model: {MODEL_PATH}")
                    model = SAM(str(MODEL_PATH))

                print("Segmenting image...")
                # One extra level of nesting = all clicks are ONE prompt for one
                # object. A flat list would make Ultralytics treat every click as
                # a separate object, so the - points would never carve the mask.
                results = model.predict(
                    source=image,
                    points=[state["points"]],
                    labels=[state["labels"]],
                    verbose=False,
                )
                result = results[0]

                if result.masks is None or len(result.masks.data) == 0:
                    print("SAM did not return a mask. Try different points.")
                    continue

                mask_index = 0
                if result.boxes is not None and len(result.boxes.conf) == len(result.masks.data):
                    mask_index = int(result.boxes.conf.argmax().item())

                mask = result.masks.data[mask_index].cpu().numpy()
                if mask.shape != image.shape[:2]:
                    mask = cv2.resize(
                        mask.astype("uint8"),
                        (image_width, image_height),
                        interpolation=cv2.INTER_NEAREST,
                    )
                mask = (mask > 0).astype("uint8") * 255
                print("Mask ready. Add more points and press Enter to refine, S to save, or Q to quit.")
            elif key == ord("s") and mask is not None:
                MASK_DIRECTORY.mkdir(parents=True, exist_ok=True)
                mask_path = MASK_DIRECTORY / f"{image_path.stem}_mask.png"
                if cv2.imwrite(str(mask_path), mask):
                    print(f"Saved object mask to: {mask_path}")
                    return mask_path
                print("Could not save the mask.")
    finally:
        cv2.destroyAllWindows()


def segment_image(image_path):
    """Load an image and interactively segment an object with SAM."""
    image_path = Path(image_path)
    image = cv2.imread(str(image_path))
    if image is None:
        print(f"Could not open image: {image_path}")
        return

    print(f"Image: {image_path}")
    print("Left click the object for positive prompts.")
    print("Right click the hand or background for negative prompts.")
    print("Use several points if the object and hand touch. Press Enter to run SAM.")
    return run_sam(image, image_path)


def main():
    try:
        image_path = choose_image_path()
    except FileNotFoundError as error:
        print(error)
        return

    segment_image(image_path)


if __name__ == "__main__":
    main()

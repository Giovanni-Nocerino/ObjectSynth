"""Capture a still image from the webcam using OpenCV."""

from datetime import datetime
from pathlib import Path

import cv2


CAMERA_INDEX = 0
CAPTURE_DIRECTORY = Path(__file__).resolve().parent / "captures"
WINDOW_NAME = "ObjectSynth - Camera"


def capture_image():
    """Open the camera and return the path of the saved photo."""
    camera = cv2.VideoCapture(CAMERA_INDEX)

    if not camera.isOpened():
        print(f"Could not open camera {CAMERA_INDEX}.")
        return

    print("Camera ready. Focus the preview window to use the keyboard.")
    print("Space: take a photo | Enter: save it | R: retake | Q: quit")

    captured_frame = None

    try:
        while True:
            if captured_frame is None:
                success, frame = camera.read()
                if not success:
                    print("Could not read a frame from the camera.")
                    break
                frame = cv2.flip(frame, 1)
                preview = frame
            else:
                preview = captured_frame.copy()
                cv2.putText(
                    preview,
                    "Photo captured - Enter: save, R: retake, Q: quit",
                    (20, 35),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.65,
                    (0, 255, 0),
                    2,
                )

            cv2.imshow(WINDOW_NAME, preview)
            key = cv2.waitKey(1) & 0xFF

            if key == ord("q"):
                break
            if key == ord(" ") and captured_frame is None:
                captured_frame = frame.copy()
                print("Photo captured. Press Enter to save or R to retake.")
            elif key in (10, 13) and captured_frame is not None:
                CAPTURE_DIRECTORY.mkdir(parents=True, exist_ok=True)
                timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                image_path = CAPTURE_DIRECTORY / f"capture_{timestamp}.jpg"
                if cv2.imwrite(str(image_path), captured_frame):
                    print(f"Saved image to: {image_path}")
                    return image_path
                else:
                    print("Could not save the image.")
                captured_frame = None
            elif key == ord("r") and captured_frame is not None:
                captured_frame = None
                print("Retake ready. Press Space when the object is in position.")
    finally:
        camera.release()
        cv2.destroyAllWindows()


def main():
    capture_image()


if __name__ == "__main__":
    main()

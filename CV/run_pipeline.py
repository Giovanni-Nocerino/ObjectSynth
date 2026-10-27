"""Run camera acquisition followed by interactive SAM segmentation."""

from acquisition import capture_image
from recognition import segment_image


def main():
    image_path = capture_image()
    if image_path is None:
        print("No photo saved. Pipeline stopped.")
        return

    segment_image(image_path)


if __name__ == "__main__":
    main()

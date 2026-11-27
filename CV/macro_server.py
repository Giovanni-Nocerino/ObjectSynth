"""Local server for the "Try your own!" lens in docs/macro-map.html.

The page sends a photo and the clicked points; this runs the same code as the
camera pipeline (recognition.segment for the SAM mask, macros.all_macros for
the macros) and sends the results back. It listens on 127.0.0.1 only.

Usage (from the repository root):
  .venv/bin/python CV/macro_server.py [--port 8765]
then open http://127.0.0.1:8765 (the page is served from here, so the
browser allows the camera), or open docs/macro-map.html from disk.

Endpoints (JSON in, JSON out):
  GET  /                                         -> docs/macro-map.html
  GET  /health                                   -> {"ok": true}
  POST /photo   {"image": <data URL>}            -> {"id", "width", "height"}
  POST /segment {"id", "points", "labels"}       -> {"mask": <PNG data URL>}
  POST /measure {"id"}                           -> {"macros": {...}}
  POST /wavetable {"id"}                         -> wavetable_story.story_data
Points are pixels of the photo as returned by /photo (long side 1920, like
the camera captures); labels are 1 for the object, 0 for the background.
"""

import argparse
import base64
import json
import tempfile
import uuid
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

import cv2
import numpy as np

import macros
import wavetable_story
from recognition import load_model, segment

PAGE_PATH = macros.CV_DIRECTORY.parent / "docs" / "macro-map.html"
PHOTO_LONG_SIDE = 1920  # the camera captures are 1920 x 1080
MAX_PHOTOS = 8          # photos kept in memory (oldest dropped)

photos = {}  # id -> {"image": BGR array, "mask": uint8 array or None}
model = None


def decode_data_url(data_url):
    data = base64.b64decode(data_url.split(",", 1)[-1])
    image = cv2.imdecode(np.frombuffer(data, np.uint8), cv2.IMREAD_COLOR)
    if image is None:
        raise ValueError("The photo could not be read.")
    return image


def to_capture_size(image):
    """Scale the long side to the camera's 1920 px, so measurements that
    depend on resolution (texture) match the presets."""
    height, width = image.shape[:2]
    scale = PHOTO_LONG_SIDE / max(width, height)
    size = (round(width * scale), round(height * scale))
    interpolation = cv2.INTER_AREA if scale < 1 else cv2.INTER_CUBIC
    return cv2.resize(image, size, interpolation=interpolation)


def add_photo(body):
    image = to_capture_size(decode_data_url(body["image"]))
    photo_id = uuid.uuid4().hex[:12]
    photos[photo_id] = {"image": image, "mask": None}
    while len(photos) > MAX_PHOTOS:
        photos.pop(next(iter(photos)))
    return {"id": photo_id, "width": image.shape[1], "height": image.shape[0]}


def get_photo(body):
    photo = photos.get(body.get("id"))
    if photo is None:
        raise ValueError("Unknown photo: send it again.")
    return photo


def segment_photo(body):
    global model
    photo = get_photo(body)
    points = [[int(x), int(y)] for x, y in body["points"]]
    labels = [int(label) for label in body["labels"]]
    if 1 not in labels:
        raise ValueError("Click the object at least once.")
    if model is None:
        model = load_model()
    mask = segment(model, photo["image"], points, labels)
    if mask is None:
        raise ValueError("SAM found no object there. Try other points.")
    photo["mask"] = mask
    ok, png = cv2.imencode(".png", mask)
    return {"mask": "data:image/png;base64," + base64.b64encode(png.tobytes()).decode()}


def with_photo_files(body, function):
    """The CV code reads files, like the pipeline: write the pair to a temp folder."""
    photo = get_photo(body)
    if photo["mask"] is None:
        raise ValueError("Segment the object first.")
    with tempfile.TemporaryDirectory() as folder:
        image_path = Path(folder) / "photo.png"
        mask_path = Path(folder) / "photo_mask.png"
        cv2.imwrite(str(image_path), photo["image"])
        cv2.imwrite(str(mask_path), photo["mask"])
        return function(image_path, mask_path)


def measure_photo(body):
    return {"macros": with_photo_files(body, macros.all_macros)}


def wavetable_photo(body):
    return with_photo_files(
        body, lambda image, mask: wavetable_story.story_data(image, mask, "your object"))


ROUTES = {"/photo": add_photo, "/segment": segment_photo, "/measure": measure_photo,
          "/wavetable": wavetable_photo}


class Handler(BaseHTTPRequestHandler):
    def send_json(self, status, payload):
        data = json.dumps(payload).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        # The page is opened from disk (origin "null"), so allow any origin;
        # the server only listens on this computer.
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(data)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Private-Network", "true")
        self.end_headers()

    def do_GET(self):
        if self.path.split("?")[0] in ("/", "/index.html") and PAGE_PATH.is_file():
            data = PAGE_PATH.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            self.wfile.write(data)
        elif self.path == "/health":
            self.send_json(200, {"ok": True})
        else:
            self.send_json(404, {"error": "Not found"})

    def do_POST(self):
        route = ROUTES.get(self.path)
        if route is None:
            self.send_json(404, {"error": "Not found"})
            return
        try:
            length = int(self.headers.get("Content-Length", 0))
            body = json.loads(self.rfile.read(length) or b"{}")
            self.send_json(200, route(body))
        except (ValueError, KeyError, TypeError) as error:
            self.send_json(400, {"error": str(error)})
        except Exception as error:  # report it to the page instead of dropping the request
            self.send_json(500, {"error": f"{type(error).__name__}: {error}"})

    def log_message(self, format, *args):
        print(f"{self.command} {self.path} -> {args[1] if len(args) > 1 else ''}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    server = HTTPServer(("127.0.0.1", args.port), Handler)
    print(f"Macro map on http://127.0.0.1:{args.port} - open it and press TRY YOUR OWN!")
    print("Ctrl+C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()

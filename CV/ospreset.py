"""Write ObjectSynth .ospreset files.

Byte-compatible with ObjectSynthAudioProcessor::getStateInformation():
JUCE copyXmlToBinary() header + the APVTS <PARAMETERS> XML, with the custom
wavetable embedded as JUCE-style base64 (MemoryBlock::toBase64Encoding).

Parameters that are not written are restored at their default value by the
synth, so a preset only needs the parameters the macros actually set.
"""

import struct
from pathlib import Path
from xml.sax.saxutils import quoteattr

import numpy as np


JUCE_MAGIC_XML = 0x21324356
FRAME_SIZE = 2048
MAX_FRAMES = 32
CUSTOM_WAVEFORM = 4  # Waveform::Custom in WavetableBank.h
EMBEDDED_FORMAT_VERSION = 2
JUCE_BASE64 = ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"


def juce_base64(data: bytes) -> str:
    """JUCE MemoryBlock::toBase64Encoding(): '<size>.' + 6-bit groups, LSB first."""
    bits = np.unpackbits(np.frombuffer(data, dtype=np.uint8), bitorder="little")
    char_count = (len(data) * 8 + 5) // 6
    bits = np.concatenate([bits, np.zeros(char_count * 6 - len(bits), dtype=np.uint8)])
    groups = bits.reshape(-1, 6) @ (1 << np.arange(6))
    table = np.frombuffer(JUCE_BASE64.encode(), dtype=np.uint8)
    return f"{len(data)}." + table[groups].tobytes().decode()


def encode_wavetable(frames: np.ndarray) -> str:
    """Embedded multiframe wavetable (format 2) as JUCE base64."""
    frames = np.asarray(frames, dtype=np.float32).reshape(-1, FRAME_SIZE)
    if not 1 <= len(frames) <= MAX_FRAMES:
        raise ValueError(f"A wavetable needs 1..{MAX_FRAMES} frames, got {len(frames)}.")
    peak = float(np.max(np.abs(frames)))
    if not np.all(np.isfinite(frames)) or peak <= 1e-6 or peak > 1.01:
        raise ValueError("Wavetable samples must be finite with a peak in (0, 1.01].")

    payload = bytearray(struct.pack("<ii", EMBEDDED_FORMAT_VERSION, len(frames)))
    for frame in frames:
        payload += b"\x00"  # no seam warning
        payload += frame.astype("<f4").tobytes()
    return juce_base64(bytes(payload))


def build_xml(parameters: dict, frames=None, wavetable_name="", frame_index=0) -> str:
    attributes = {}
    if frames is not None:
        attributes["selectedWaveform"] = str(CUSTOM_WAVEFORM)
        attributes["customWavetableName"] = wavetable_name
        attributes["customFrameIndex"] = str(int(frame_index))
        attributes["customWavetableData"] = encode_wavetable(frames)

    root = "".join(f" {key}={quoteattr(value)}" for key, value in attributes.items())
    params = "".join(
        f"<PARAM id={quoteattr(name)} value={quoteattr(repr(float(value)))}/>"
        for name, value in sorted(parameters.items())
    )
    return f'<?xml version="1.0" encoding="UTF-8"?> <PARAMETERS{root}>{params}</PARAMETERS>'


def write_preset(path, parameters: dict, frames=None, wavetable_name="", frame_index=0):
    """Write an .ospreset that ObjectSynth's OPEN PRESET (and the host) can load."""
    text = build_xml(parameters, frames, wavetable_name, frame_index).encode("utf-8")
    data = struct.pack("<II", JUCE_MAGIC_XML, len(text) + 1) + text + b"\x00"
    if len(data) > 2 * 1024 * 1024:
        raise ValueError("Preset larger than the synth's 2 MB limit.")
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return path

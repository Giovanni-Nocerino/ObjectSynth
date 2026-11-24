"""Macro (0..1) -> ObjectSynth parameters.

Each parameter listens to one macro, moves inside a musically safe range and
uses a curve: linear, exponential (times and frequencies) or a threshold
(switching a module on). Parameters not listed keep the synth's neutral init.
"""


def linear(macro, start, end):
    return start + (end - start) * macro


def exponential(macro, start, end):
    """Equal ratios for equal macro steps (how we hear time and frequency)."""
    return start * (end / start) ** macro


def ease_out(macro, start, end, power=0.5):
    """Rises quickly at first, then flattens: mid values already have a strong effect."""
    return start + (end - start) * macro ** power


def ease_in(macro, start, end, power=3.0):
    """Starts slowly, grows mostly near the top: only high values have a strong effect."""
    return start + (end - start) * macro ** power


def threshold(macro, at):
    return 1.0 if macro >= at else 0.0


def gated_linear(macro, at, start, end, below=0.0):
    """`below` under the threshold, then linear from `start` (at `at`) to `end` (at 1)."""
    if macro < at:
        return below
    return linear((macro - at) / (1.0 - at), start, end)


def sharpness_parameters(s):
    """Spigolosita: soft/round objects stay clean, sharp ones bite and distort."""
    return {
        # Distortion switches on early and drives hard: the synth's saturator gives
        # ~3% THD at drive 3 and ~40% at drive 10, so the top of the range is needed.
        "distortionEnabled": threshold(s, 0.15),
        "distortionDrive": ease_out(s, 2.0, 10.0),
        # Faster attack for sharper objects: 50 ms -> 2 ms.
        "attack": exponential(s, 0.050, 0.002),
        # A filter-envelope "pluck" at note start, stronger for sharper objects.
        "filterEnvEnabled": threshold(s, 0.3),
        "filterEnvAmount": linear(s, 0.0, 3.0),
    }


def brightness_parameters(b):
    """Luminosita: dark objects sound muffled, bright objects open and airy."""
    return {
        # Filter Bank (global tone): only the low-pass branch, opened by brightness.
        "filterBankEnabled": 1.0,
        "lpMix": 1.0,
        "hpMix": 0.0,
        "bpMix": 0.0,
        "lpCutoff": exponential(b, 500.0, 18000.0),
        # Voice filter: where each note settles after the Spigolosita "bite".
        "voiceFilterCutoff": exponential(b, 400.0, 4000.0),
    }


def stepped(macro, start, end):
    """Whole-number steps (e.g. how many oscillators)."""
    return float(round(start + (end - start) * macro))


PLUCK_BELOW = 0.3  # Corpo under this: sustain 0, a pluck


def body_parameters(c):
    """Corpo: small objects sound thin and short, big ones thick, wide and long."""
    return {
        # Unison: more oscillators spread wider in stereo (a bigger "ensemble").
        # Their detune belongs to Movimento (asymmetry).
        "unisonEnabled": threshold(c, 0.2),
        "unisonCount": stepped(c, 1, 7),
        "unisonSpread": linear(c, 0.2, 1.0),
        # Small objects are plucks: no sustain, the note dies after the decay.
        # From PLUCK_BELOW up the note holds, more for bigger objects.
        "sustain": gated_linear(c, PLUCK_BELOW, 0.4, 0.9),
        # Bigger objects ring longer: a longer pluck tail and a longer release.
        "decay": exponential(c, 0.2, 1.2),
        "release": exponential(c, 0.1, 1.5),
    }


# LFO rates offered by the texture, slow -> fast. Indices into the synth's
# lfoSync choice list (DSP/TempoSync.h): 1/1, 1/2, 1/4, 1/8, 1/16.
LFO_SYNC_STEPS = [2, 3, 6, 9, 12]
LFO_DESTINATION_LP_CUTOFF = 1  # ModDestination::LPCutoff
LFO_SINE = 0


def movement_parameters(asymmetry, texture, colorfulness):
    """Movimento: each ingredient moves its own part of the sound."""
    step = LFO_SYNC_STEPS[min(int(texture * len(LFO_SYNC_STEPS)), len(LFO_SYNC_STEPS) - 1)]
    return {
        # Asymmetry -> detune: a lopsided object gives out-of-tune, beating voices.
        # Never 0: identical unison voices would only sound phasey, not wider.
        "unisonDetune": linear(asymmetry, 3.0, 20.0),  # cents
        # Texture -> LFO on the low-pass cutoff: busy surfaces pulse, faster and deeper.
        "lfoEnabled": threshold(texture, 0.2),
        "lfoSync": float(step),
        "lfoWaveform": float(LFO_SINE),
        "lfoRoute1Destination": float(LFO_DESTINATION_LP_CUTOFF),
        "lfoRoute1Depth": linear(texture, 0.1, 0.6),
        # Colourfulness -> chorus: vivid colours give a richer, moving sound.
        # Only clearly colourful objects get it (skin is ~0.6), and gently:
        # the mix stays low until the most vivid colours.
        "chorusEnabled": threshold(colorfulness, 0.5),
        "chorusMix": ease_in(colorfulness, 0.08, 0.30),
        "chorusDepth": linear(colorfulness, 0.15, 0.45),
    }


def preset_parameters(macros):
    """All parameters set by the available macros."""
    parameters = {}
    if "sharpness" in macros:
        parameters.update(sharpness_parameters(macros["sharpness"]))
    if "brightness" in macros:
        parameters.update(brightness_parameters(macros["brightness"]))
    if "body" in macros:
        parameters.update(body_parameters(macros["body"]))
    if {"asymmetry", "texture", "colorfulness"} <= macros.keys():
        parameters.update(movement_parameters(
            macros["asymmetry"], macros["texture"], macros["colorfulness"]))
    return parameters

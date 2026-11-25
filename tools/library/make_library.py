"""
Writes CoolSynth 303's built-in library (resources/library/library303.json): lines and sounds on separate shelves
(CoolSynth's D338). The lines are original, written in the manner of each style from published descriptions of how
those styles are built (MusicRadar's and Roland's TB-303 guides among them), named our own way; none is a record.

    python -I tools/library/make_library.py

A line's steps in a short notation, one token a step:
    C C# D D# E F F# G G# A A# B ^C   the note (^C the C above, the step's note 12)
    + or -                            an octave up or down, after the note
    a, s                              accent, slide into the next step, after that
    .                                 a rest;  ~ a tie (the note before held on)
Standard library only.
"""
import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "resources", "library", "library303.json")
NOTES = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11, "^C": 12}
TOKEN = re.compile(r"^(\^C|[A-G]#?)([+-]?)([as]*)$")


def steps(tokens):
    out, last = [], {"note": 0, "octave": 0}
    for t in tokens:
        if t == ".":
            out.append({"note": last["note"], "octave": last["octave"], "accent": False, "slide": False, "gate": "rest"})
            continue
        if t == "~":
            out.append({"note": last["note"], "octave": last["octave"], "accent": False, "slide": False, "gate": "tie"})
            continue
        m = TOKEN.match(t)
        if not m:
            raise ValueError("not a step: " + t)
        note, octave, flags = NOTES[m.group(1)], {"": 0, "+": 1, "-": -1}[m.group(2)], m.group(3)
        last = {"note": note, "octave": octave}
        out.append({"note": note, "octave": octave, "accent": "a" in flags, "slide": "s" in flags, "gate": "note"})
    while len(out) < 16:
        out.append({"note": 0, "octave": 0, "accent": False, "slide": False, "gate": "rest"})
    return out


def line(name, style, bpm, tokens, sound, about):
    return {"name": name, "style": style, "bpm": bpm, "sound": sound, "about": about,
            "pattern": {"name": name, "length": len(tokens), "steps": steps(tokens), "motion": {}}}


S = str.split
LINES = [
    # Chicago '87: sparse, the upper octave on the accents, a slide or two, 120 to 125 BPM.
    line("Warehouse 87", "Chicago 87", 122, S("Cas C+ ~ C D# . C+a C . Cas A#- C . G- C+as C"), "Classic Acid",
         "Sparse, the accents up an octave, a slide into each half."),
    line("Jack Step", "Chicago 87", 124, S("C C . C+as C D# C ~ Ca C . F C G-s C ."), "Classic Acid",
         "A jacking root with one leap up and a slide down at the end."),
    line("Sixteen Strong", "Chicago 87", 120, S("Ca C+ C D#s F C C+a A# C C+s G C Ca D# F#s G"), "Squelch",
         "Every step a note, the blue note sliding into the fifth."),
    # Acid house: the full 16, the root hammered, accents and slides as the hooks, 125 BPM.
    line("Basement Hook", "Acid house", 125, S("Cas C C+ C D#a C Ds D# C C+as C G- C A#-a C C+"), "Squelch",
         "Root and octave trading, a chromatic slide in the middle."),
    line("Downstairs", "Acid house", 124, S("C Ca C . D# Ca F F#s G C+a G . F D#a D C"), "Classic Acid",
         "Up the minor scale to the octave and back down."),
    line("Strobe Corridor", "Acid house", 126, S("C- C C-a C C- C+s C C- C-a D# C- C+ C-as A#- C- G-"), "Warehouse Drive",
         "Low and high C in turns, the low ones accented."),
    # Long slides: dense G lines that glide nearly every step, 133 BPM.
    line("Long Glide", "Long slides", 133, S("Gs G+s Gs F Gs G+as G ~ Gs A#s G F Gas G+s D F"), "Classic Acid",
         "Gliding up and down the octave on G, barely a hard step."),
    line("Rolling G", "Long slides", 134, S("G Gs G+ G Gas Fs G G+s G D#s F G Gas G+ G A#s"), "Squelch",
         "Every other step slides, the accents on the turns."),
    line("Bounce Glide", "Long slides", 132, S("C C+s C C Cas D#s C C+ C Gs A# C+a C D#s F Gs"), "Warehouse Drive",
         "A bouncing root that slides up through the fifth to the octave."),
    # Acid techno: a 12-step line cycling over the four-on-the-floor kick, 135 BPM and up.
    line("Twelve Cycle", "Acid techno", 136, S("Ca C C+ C D#s C Ca C+ A#- C G-s C"), "Warehouse Drive",
         "Twelve steps: the accent falls somewhere new each bar."),
    line("Rave Engine", "Acid techno", 138, S("C Ca C C C+as C C D# C Ca C F# C C+s C Ga"), "Squelch",
         "A driving root with the tritone, accents pushing the off-beats."),
    line("Hammer Twelve", "Acid techno", 140, S("C-a C C- C+ C- C C-as D# C- C C-a A#-"), "Devil Screamer",
         "Twelve low steps hammered, the octave jumping in."),
    # Hypnotic: odd lengths (9, 11, 13 steps) turning against the bar, short decays, 126 BPM.
    line("Nine", "Hypnotic", 126, S("Ca C . C+ C . D#s C ."), "Minimal Click",
         "Nine steps, so it lands differently every bar."),
    line("Eleven", "Hypnotic", 126, S("C C+a C . C Gs C . Ca C+ ."), "Minimal Click",
         "Eleven steps, an octave leap and a slide into the fifth."),
    line("Thirteen", "Hypnotic", 124, S("Ca . C C D# . C+as C . C Fa . C"), "Classic Acid",
         "Thirteen steps with room between the notes."),
    # Trance stabs: a note on every sixteenth, accents an octave up, 138 BPM.
    line("Higher Stabs", "Trance", 138, S("C C+a C C C C+a C D# C C+a C C F C+a G C"), "Trance Stab",
         "Every sixteenth, the accents jumping the octave."),
    line("Gate Runner", "Trance", 140, S("C+a C C C+ C C+a C C C+ C A#a C C+ C Ga C"), "Trance Stab",
         "Octave stabs running, the accents walking down."),
    line("Rising Four", "Trance", 136, S("C C C+a C D# D# D#+a D# F F F+a F G G G+as A#"), "Trance Stab",
         "Four notes a beat, the root climbing each beat."),
    # Psy rolling: the kick on the beat, the bass on the three sixteenths after it, 145 BPM.
    line("Night Gallop", "Psy", 145, S(". C C C . C C C . C C C . C C+a C"), "Minimal Click",
         "The rolling gallop: rests on the beat, where the kick sits."),
    line("Forest Floor", "Psy", 145, S(". Ca C C . C C# C . Ca C C . C F D#"), "Minimal Click",
         "The gallop with the flat second, accents after the kick."),
    line("Phrygian Roll", "Psy", 143, S(". C C# C . C Ca C . C C# C . G F C#a"), "Devil Grit",
         "Rolling on C and D flat, a turn down at the end."),
    # Electro: syncopated, room in it, the square wave, 118 BPM.
    line("Robot Steps", "Electro", 118, S("Ca . . C ~ . D# . C . Ca ~ G- . A#- ."), "Rolling Sub",
         "Syncopated and dry, held notes against rests."),
    line("Breakdance", "Electro", 116, S("C . C+a . . C . D#s F . C . Ca . A#- G-"), "Liquid Lead",
         "Broken rhythm, a slide into the fourth."),
    line("Planet Wire", "Electro", 120, S("Ca ~ . C . C+ . C D#a ~ . D# F . G ."), "Rolling Sub",
         "Pairs of notes climbing, each with a held accent."),
    # Liquid lead: the square wave with long slides, a melody more than a bass, 128 BPM.
    line("Liquid Glide", "Liquid lead", 128, S("Cs D#s Gs C+ ~ A#s Gs F ~ D#s Fs G ~ C+s A# G"), "Liquid Lead",
         "Up the minor chord to the octave and gliding back."),
    line("Mercury", "Liquid lead", 126, S("G ~ A#s C+ ~ ~ A#s G F ~ Gs D# ~ Fs G ~"), "Liquid Lead",
         "Long held notes joined by slides."),
    line("Night Drive", "Liquid lead", 124, S("C+as D#+ ~ C+ A#s G ~ F Gas A# ~ G Fs D# ~ C"), "Devil Sustain",
         "Falling from the high octave, sliding between the steps."),
    # Rolling sub: the square, the filter nearly closed, low notes, 124 BPM.
    line("Deep Roll", "Rolling sub", 124, S("C- C- C C- C- C-s D#- C- C- C- C C- C- A#- G-s A#-"), "Rolling Sub",
         "Low C rolling, a lift to the middle C now and then."),
    line("Low Ride", "Rolling sub", 122, S("C-a . C- C- C . C- C-s D#- . C- C- F- . D#- C-"), "Devil Muffled Bass",
         "A riding sub with rests to breathe."),
    line("Underground", "Rolling sub", 120, S("C- C- ~ C- D#- ~ C- ~ F- ~ D#- C- G- ~ F- D#-"), "Rolling Sub",
         "Held low notes walking up and down the scale."),
]


def sound(name, family, about, **panel):
    base = {"square": False, "driveOn": False, "delayOn": False, "devilFish": False}
    base.update(panel)
    return {"name": name, "family": family, "about": about, "panel": base}


# cutoff and envMod are on the pots' track since CoolSynth's D364 (a 50k A taper): each was set anew so the pot sits
# where it did on the exponential track the sounds were made on, and the sound plays as before.
SOUNDS = [
    sound("Classic Acid", "Stock", "Saw, the filter low and resonant, a medium envelope: the classic squelch.",
          cutoff=0.3308, resonance=0.80, envMod=0.5203, decay=0.50, accent=0.70),
    sound("Squelch", "Stock", "Darker and more resonant, a wide envelope that the accents fling open.",
          cutoff=0.2643, resonance=0.90, envMod=0.6479, decay=0.35, accent=0.85),
    sound("Warehouse Drive", "Stock", "The classic squelch through the drive, pushed.",
          cutoff=0.3931, resonance=0.72, envMod=0.5444, decay=0.55, accent=0.70, driveOn=True, drive=0.60, tone=0.55),
    sound("Minimal Click", "Stock", "A short decay and the filter open: clicks and plucks for odd lengths.",
          cutoff=0.5203, resonance=0.50, envMod=0.3931, decay=0.10, accent=0.50),
    sound("Rolling Sub", "Stock", "The square with the filter nearly closed and little resonance: a round sub.",
          square=True, cutoff=0.1659, resonance=0.20, envMod=0.2643, decay=0.60, accent=0.40),
    sound("Liquid Lead", "Stock", "The square, half open, a little delay: a gliding lead.",
          square=True, cutoff=0.4419, resonance=0.60, envMod=0.3521, decay=0.70, accent=0.50, delayOn=True, feedback=0.35, mix=0.25),
    sound("FX Chirp", "Stock", "Resonance full and a short sweep: chirps and zaps.",
          cutoff=0.3177, resonance=1.00, envMod=0.6962, decay=0.30, accent=0.90),
    sound("Trance Stab", "Stock", "Short and bright with drive and an eighth-note delay: stabs.",
          cutoff=0.4999, resonance=0.65, envMod=0.5731, decay=0.20, accent=0.80, driveOn=True, drive=0.40, delayOn=True,
          delayDivision=1, feedback=0.30, mix=0.25),
    sound("Devil Screamer", "Devil Fish", "Overdrive and the high-resonance accent sweep: it screams on the accents.",
          devilFish=True, cutoff=0.3521, resonance=0.85, envMod=0.5444, decay=0.50, accent=0.80, overdrive=0.70,
          accentSweep=1, filterFm=0.30),
    sound("Devil Sustain", "Devil Fish", "Long decays, a soft attack and slow slides: notes that sing.",
          devilFish=True, cutoff=0.3931, resonance=0.70, envMod=0.3931, decay=0.90, accent=0.60, normalDecay=0.90,
          accentDecay=0.50, softAttack=0.30, slideTime=0.40),
    sound("Devil Grit", "Devil Fish", "Heavy Filter FM and tracking: bright and gritty, as Whittle's own.",
          devilFish=True, cutoff=0.4999, resonance=0.90, envMod=0.3177, decay=0.60, accent=0.60, overdrive=0.50,
          filterFm=0.80, tracking=0.50),
    sound("Devil Muffled Bass", "Devil Fish", "Overdriven into the Muffler: a fat, squashed bass.",
          devilFish=True, cutoff=0.2887, resonance=0.60, envMod=0.4999, decay=0.40, accent=0.70, overdrive=0.90, muffler=1),
]


def main():
    names = {s["name"] for s in SOUNDS}
    for ln in LINES:
        assert ln["sound"] in names, ln["sound"]
        assert 1 <= ln["pattern"]["length"] <= 16, ln["name"]
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump({"format": "CoolSynth303 library", "version": 1, "lines": LINES, "sounds": SOUNDS}, f, indent=1)
    print("%d lines in %d styles, %d sounds" % (len(LINES), len({ln["style"] for ln in LINES}), len(SOUNDS)))


if __name__ == "__main__":
    main()

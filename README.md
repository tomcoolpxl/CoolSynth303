# CoolSynth 303

A bass line synthesizer for Windows based on the circuits of the Roland TB-303: its oscillator, filter, envelope and
VCA modelled part by part and solved as circuits, played by a step sequencer, then a drive and a delay. It is the
sibling of CoolSynth 808, built on the same engine.

TB-303 is a trademark of Roland Corporation. CoolSynth 303 is not made, approved or endorsed by Roland; it is based on
the published service notes of the machine and checked against ngspice renders of its circuit and recordings of a
real unit.

## Playing it

The panel, from the top:

- The top bar: the pattern you edit between two arrows, New, Open and Save; Play (or Space), the tempo (drag it or
  use the wheel, double-click for 128), Tap and Rec. Rec writes the knobs you turn while it plays into the pattern.
- The sections in the order the sound flows: the oscillator (Tuning, Saw or Square), the filter (Cutoff, Resonance,
  Env mod), the envelope (Decay, Accent), the amplifier (Volume), then Drive (On, Amount, Tone) and Delay (On, Time
  in notes, Feedback, Mix). While a pattern plays, a knob the pattern moves shows where it is moved to.
- The keyboard, three octaves: the octave down, as written and up. In Write, a key sets the picked step's note and
  octave and moves to the next step; Tie and Rest do the same for a held or a silent step, Back and Step move the
  pick, Accent and Slide switch the picked step's own. In Transpose, a key shifts the running pattern (the middle C
  plays it as written; right-click Transpose to reset it). While stopped a key also sounds. The computer's keys Z to
  comma play the middle octave.
- The sequencer:
  - The patterns 01 to 16, each with an A and a B. Click one to edit it and hear it: while playing it comes in when
    the one that plays reaches its end.
  - Chain: switch it on to play the chain instead. Its blocks are patterns with their repeats; click a block to edit
    that pattern, Shift-click or wheel for repeats, right-click to remove it; + adds the pattern you edit. Loop plays
    the chain round, or once and then stops.
  - The tools: Generate (a new line in the key), Mutate (four steps changed; the arrow beside it says what may
    change), the key (root and scale) and Snap to key, Move (the pattern a step earlier or later), Pitch (every note a
    semitone), Length (click for 16, 12 or 8 steps, wheel for any), Undo and Redo (also Ctrl+Z and Ctrl+Shift+Z).
  - The grid: drag a step up or down in the pitch lane (or wheel it) for its note; the notes run down the lane's
    left edge. Below it each step's octave, accent, slide and gate (note, tie or rest). Right-click a step for more.
  - The motion lane: the recorded moves of one knob (click its name for the next); draw them, right-click to clear.
  - Locks, on the right: turn a knob there to hold its value on the picked step.
  - Drag MIDI: drag it into your music software and the edited pattern goes along as a MIDI file (ties held, a slide
    as an overlap, accents at full velocity); click it to save the file instead.
- Mods, at the top right, opens the trimmers inside the case over the synth's sections: TM3 (how far the cutoff
  reaches), TM4 (the tune), TM5 (the width of an octave) and C11 (the square's shaper), as fitted to a real unit;
  Stock sets them back.
- Beside them, the Devil Fish: Robin Whittle's well-known modification of the 303, switched on or off (off is the
  stock 303, untouched). On, it adds Overdrive, Slide Time, Soft Attack, Normal and Accent Decay, Filter Tracking,
  Filter FM, the Sweep Speed, Accent Sweep and Muffler switches and the red Accent button (every note accented while
  held). Cut Off and Env Mod reach further, the bass is fuller, and the panel's Decay then sets the volume's decay
  (held notes past half). Its Stock sets the knobs where it sounds most like a 303. Devil Fish is Whittle's name; this
  is a model of his mod from his manual, not his product.

- Lid, beside Mods, lifts the lid: the whole voice as one schematic sheet, every part, the current on its wires as
  moving dots and their voltage as a green or red tint, live from the circuit as it plays. Wheel to zoom, drag to
  pan, click a part to read it; Slow replay holds the last two seconds and walks them fifty times slower. Escape or
  Back to the panel closes it.

MIDI: while stopped, a note plays the voice (the middle C at MIDI note 36); while playing, a note transposes the
pattern from middle C (note 60). It follows a MIDI clock when one comes in (its start, stop, continue, song position
and tempo, the top bar then showing CLOCK), and sends its own while it plays when none does. The audio and MIDI
settings are in the Options menu of the window.

## How the sound is made

The voice is the engine's TB-303 circuit (`external/coolsynth-engine/src/synth/v3/Tb303Voice.h`): the oscillator's
exponential converter and its capacitor's reset, the square's one-transistor shaper, the diode ladder with its drive
and read-out, the envelope generator and the accent's circuit, and the BA662 VCA, each as its transistors. It runs at
twice the output's rate (96 kHz at 48 kHz) and is matched to ngspice renders of the service notes' circuit within a
fraction of a cent and about half a decibel. The sequencer plays it as the original's CPU does: six clocks a step,
the gate on for three of them, a slide holding it into the next note, accent and slide latched at the step's start,
the gate 2 ms behind the clock.

The Devil Fish is the same circuit with the parts Whittle changed set to his values, a current into the filter's
control for tracking and FM, and a sustain on the volume envelope; Soft Attack and the Muffler, whose circuits he
never published, are simple stand-ins after the VCA.

The output ends in a soft saturation: untouched up to -3 dBFS, above it rounded off toward full scale, so a pushed
Devil Fish never clips digitally.

The standalone app serves Claude over MCP on 127.0.0.1:47633 (`src/mcp`, as CoolSynth's): read and set the panel, run
the sequencer, hold keys, record the output into a WAV file, picture the window. The plugin opens no port.

`tools/listening-pass` plays the stock voice through Din Sync's 25 recorded sets of a real TB-303 (each knob from
25 to 100 %, the others at five settings) and serves a page that switches between the real unit and ours while both
play. The recordings are not in the repository; they come from dinsync.info.

## Building from source

Windows, Visual Studio 2022, CMake 3.22 or later.

```
git clone --recurse-submodules https://github.com/tomcoolpxl/CoolSynth303.git
cmake -S CoolSynth303 -B CoolSynth303/build -G "Visual Studio 17 2022" -A x64
cmake --build CoolSynth303/build --config Debug --parallel
ctest --test-dir CoolSynth303/build -C Debug --output-on-failure
```

The Debug build is compiled optimised (the engine's `cmake/CoolSynthFlags.cmake`). The app is
`build/CoolSynth303_artefacts/Debug/Standalone/CoolSynth 303.exe`; the plugin is
`build/CoolSynth303_artefacts/Debug/VST3/CoolSynth 303.vst3` (copy it to `C:\Program Files\Common Files\VST3`).

In a DAW the host's transport runs the sequencer: it plays while the host plays, from where it is, at its tempo
(the top bar shows HOST), and a loop in the host starts the pattern again; the save file is the host's project.

MIT licence. The fonts are Archivo and IBM Plex Mono, under the SIL Open Font License (`resources/fonts`).

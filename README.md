# CoolSynth 303

A bass line synthesizer for Windows based on the circuits of the Roland TB-303: its oscillator, filter, envelope and
VCA modelled part by part and solved as circuits, played by a step sequencer, then a drive and a delay. It is the
sibling of CoolSynth 808, built on the same engine.

TB-303 is a trademark of Roland Corporation. CoolSynth 303 is not made, approved or endorsed by Roland; it is based on
the published service notes of the machine and checked against ngspice renders of its circuit and recordings of a
real unit.

## Install

Get the latest `CoolSynth303-Setup` installer from the [Releases page](https://github.com/tomcoolpxl/CoolSynth303/releases).
It runs on 64-bit Windows 10 or 11.

Windows may warn that the publisher is unknown, because the installer isn't code-signed: click More info, then Run
anyway.

The installer puts the app in `C:\Program Files\CoolSynth 303` with a Start menu entry, and the VST3 in
`C:\Program Files\Common Files\VST3`; it installs both, the app only or the VST3 only. Uninstalling it (Windows'
Installed apps) removes them and leaves your own lines and sounds in `%APPDATA%\CoolSynth303\Library` and the app's
settings.

The first time the app starts, Windows asks whether it may use the network. Allow it on private networks (your home's):
the beat sync needs that to play in time with other computers.

The release page also has a zip with the app, the VST3 and the notices, for those who would rather copy them by hand.

## Playing it

The panel, from the top:

- The top bar: the pattern you edit between two arrows, New, Open and Save; Play (or Space), the tempo (drag it or
  use the wheel, double-click for 128), SYNC, Tap and Rec. Rec writes the knobs you turn while it plays into the
  pattern.
- The sections in the order the sound flows: the oscillator (Tuning, Saw or Square), the filter (Cutoff, Resonance,
  Env mod), the envelope (Decay, Accent), the amplifier (Volume), then Drive (On, Amount, Tone) and Delay (On, Time
  in notes, Feedback, Mix). While a pattern plays, a knob the pattern moves shows where it is moved to.
- Volume is the 303's own VOLUME pot, ahead of its output mixer: turned up high, loud accents flatten as a real
  unit's do, and Drive gets what Volume lets through.
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
  - Length, at the pattern row's end: click for 16, 12 or 8 steps, wheel for any.
  - The column beside the grid has two tabs. Line: Generate (a new line in the key), Mutate (four steps changed; the
    arrow beside it says what may change), the key (root and scale) and Snap to key, Move (the pattern a step earlier
    or later), Pitch (every note a semitone). Locks: the picked step's knobs.
  - Undo, Redo and Drag MIDI sit at the sequencer's top right (Undo and Redo also Ctrl+Z and Ctrl+Shift+Z).
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
and tempo, the top bar then showing CLOCK), and sends its own while it plays when none does.

The beat sync plays the 303 in time with CoolSynth 808 and the other apps and DAWs that have it, on this computer or
the network (the standalone app only; in a DAW the DAW's transport leads):
- on from the start; SYNC beside the tempo lights while on and counts the others ("SYNC 2"), a click switches it
- with others, the tempo is shared (the top bar shows SYNC) and a Play starts on their next bar line; Play and Stop
  go to all of them, unless Options, MIDI, "Share Play and Stop" is off
- the first time, Windows asks about its firewall: allow private networks to sync with other computers

For hardware it follows an incoming MIDI clock and sends its own, also while the beat sync leads. While the beat sync
has others with it, an incoming clock is ignored. The 303 and the 808 together take some two and a half cores; a busy
program beside them can make the 808 stutter.

Options, in the top bar, opens the 303's own options: the audio device and the accuracy, the MIDI ports, the beat
sync's Play and Stop and Claude's MCP server, the window's size and theme, and the WAV export (in a DAW, the Host page
has the accuracy).
- Accuracy: Best, or Light: the voice at the output's own rate and solved a little less tightly, some 40 % less CPU,
  the top octave up to 3.5 dB duller. A DAW's export always renders at Best.
- Export: the pattern shown or the chain, 1 to 16 times, cut, rung out or as a seamless loop, the mix or the dry
  voice beside it too, at the app's rate or 96 kHz; a 24-bit WAV rendered at Best while the 303 plays on.

## How the sound is made

The voice is the engine's TB-303 circuit (`external/coolsynth-engine/src/synth/v3/Tb303Voice.h`): the oscillator's
exponential converter and its capacitor's reset, the square's one-transistor shaper, the diode ladder with its drive
and read-out, the envelope generator and the accent's circuit, the BA662 VCA, and the output stage (VOLUME into the
two-transistor mixer, out at the OUTPUT jack into a recorder's 16k), each as its transistors. The mixer can pull
its output down only through a 10k resistor, so with Volume high the loudest notes clip on that side, as Din Sync's
recordings of a real unit do. It runs at
twice the output's rate (96 kHz at 48 kHz; at the output's own in Light) and is matched to ngspice renders of the service notes' circuit within a
fraction of a cent and about half a decibel. The sequencer plays it as the original's CPU does: six clocks a step,
the gate on for three of them, a slide holding it into the next note, accent and slide latched at the step's start,
the gate 2 ms behind the clock.

The Devil Fish is the same circuit with the parts Whittle changed set to his values, a current into the filter's
control for tracking and FM, and a sustain on the volume envelope; Soft Attack and the Muffler, whose circuits he
never published, are simple stand-ins after the VCA, ahead of Volume.

The Library key opens a shelf of lines and sounds: 30 original lines in ten styles (Chicago 87 to rolling sub) and
12 sounds, four of them Devil Fish ones. Audition plays a line in place of the pattern you edit, or your pattern with
a sound; Load puts it in, one undo step; Keep a copy puts one among yours (kept in
`%APPDATA%\CoolSynth303\Library`), which you can rename or send to the Recycle Bin. `tools/library/make_library.py`
writes the built-in ones.

The output ends in a soft saturation: untouched up to -3 dBFS, above it rounded off toward full scale, so a pushed
Devil Fish never clips digitally.

The standalone app can serve Claude over MCP on 127.0.0.1:47633 (`src/mcp`, as CoolSynth's): read and set the panel,
run the sequencer, hold keys, record the output into a WAV file, picture the window. It is off until switched on in
Options, MIDI, Claude (MCP server), and the switch starts or stops it at once; only this computer can reach it. The
switch is kept in `%APPDATA%\CoolSynth303\CoolSynth303.settings` as `<VALUE name="mcpServer" val="1"/>`, so a test
setup can switch it on there before the app starts. `COOLSYNTH303_NO_MCP` in the environment keeps it off whatever the
switch says. The plugin opens no port.

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

Use `--config Release` for a build to give to someone else: it carries the C++ runtime inside it.
`scripts/ci/BuildAndTest.ps1 -Configuration Release -RunTests`, then
`scripts/ci/PackageRelease.ps1 -Configuration Release -Installer`, build, test and package as the release does (the
installer needs Inno Setup 6.5 or newer, `installer/CoolSynth303.iss`). A version tag (`v0.1.0`) makes GitHub build
and publish a release.

In a DAW the host's transport runs the sequencer: it plays while the host plays, from where it is, at its tempo
(the top bar shows HOST), and a loop in the host starts the pattern again; the save file is the host's project.

MIT licence. The fonts are Archivo and IBM Plex Mono, under the SIL Open Font License (`resources/fonts`). It's built
with [JUCE](https://juce.com) 9 under the free Starter licence. `THIRD_PARTY_NOTICES.txt`, which the installer puts
beside the app, holds the notices of what it carries: JUCE's image, font and audio libraries, the VST3 SDK, Open303
and the fonts (`tools/release/make_notices.py` writes it).

# CoolSynth 303 Project Rules

The shared project memory for every AI tool (`CLAUDE.md` points here).

## What it is

- A bass line app for Windows (JUCE 9.0.5, C++20, CMake): a JUCE audio plugin built as a Standalone and a VST3
  (D305); in a DAW the host's transport runs the sequencer.
- The sound is the engine's TB-303 circuit, the submodule `external/coolsynth-engine` (its own repository,
  `tomcoolpxl/coolsynth-engine`): `Tb303Voice` (CoolSynth's D295), at twice the host's rate, then the engine's
  `DriveProcessor` and `GlobalDelay`. The sequencer is the app's own (`src/Sequencer`), as the original's CPU plays.
- The design, the user's decisions and the history live in CoolSynth's repository (`E:\github\CoolSynth`):
  `docs/design/COOLSYNTH_303.md`, `docs/research/tb-303-app.md`, `DECISIONS_LOG.md` from D275, and
  `docs/design/mockups/coolsynth-303-layouts.html`, the look's reference (the layout the user picked is 2.3, D302).
  Log new decisions there, in the same numbering.

## Where things are

- `src/Pattern`: a step, a pattern, the bank, the chain, the panel's values, the project as JSON; the tools
  (generate, mutate, snap to key, shift, semitones).
- `src/Sequencer`: plays a pattern or the chain step by step on the audio thread: the gate's timing, slides, ties,
  the queue, locks and recorded moves per step.
- `src/Processor`: the voice, the sequencer, the drive and delay, MIDI in, MIDI clock in (`src/ClockFollower`,
  the 808's) and out, a host's transport, undo; the project lives on the message thread and reaches the audio thread
  through a lock the audio thread only tries. The circuit rests while stopped and silent.
- `src/MidiExport`: a pattern as a MIDI file (Drag MIDI).
- `src/Editor` and `src/ui`: the panel at 1280 by 800, scaled to the window: the top bar and the synth's sections
  (`ui/Panel`), the three-octave keyboard (`ui/Keyboard`), the pattern row, step grid and the inspector column (Locks and Line tabs)
  (`ui/SequencerView`), the widgets and the machined look, graphite plates and knurled knobs (`ui/Widgets`, `ui/Theme`; CoolSynth's D348).
- `tests/`: `CoolSynth303Tests`, the built VST3 hosted as a DAW would (`HostingTests`).
  `COOLSYNTH303_SNAPSHOTS=<folder>` makes the editor's test write pictures of the panel there;
  `COOLSYNTH303_RENDER=<folder>` makes the processor's write listening files.

## Rules

- The rules of CoolSynth's `GEMINI.md` hold here: pick lists for the user's choices, research online first, plain
  words for anything a player sees, no heredocs, commit on `main` and push after each verified chunk.
- An engine change goes to the engine repository (its `main`), then the new submodule pointer is committed here and
  in CoolSynth.
- Required checks: the Debug build (compiled optimised) and `ctest --test-dir build -C Debug --output-on-failure`.

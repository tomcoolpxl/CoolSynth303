# CoolSynth 303 manual

The panel, from the top.

## The top bar

The pattern you edit between two arrows, then New, Open and Save. Library opens the shelf of lines and sounds, Options the app's options. Init sets the sound back to its defaults (every knob and switch, the Devil Fish off, the trimmers stock), leaving the patterns, as one undo step.

Play (or Space), the tempo (drag it or use the wheel, double-click for 128), SYNC (the beat sync, see below), Tap and Rec. Rec writes the knobs you turn while it plays into the pattern.

Lid lifts the lid, Mods opens the trimmers and the Devil Fish.

## The sections

In the order the sound flows: the oscillator (Tuning, Saw or Square), the filter (Cutoff, Resonance, Env mod), the envelope (Decay, Accent), the amplifier (Volume), then Drive (On, Amount, Tone) and Delay (On, Time in notes, Feedback, Mix). While a pattern plays, a knob the pattern moves shows where it is moved to.

Volume is the 303's own VOLUME pot, ahead of its output mixer: turned up high, loud accents flatten as a real unit's do, and Drive gets what Volume lets through.

## The keyboard

Three octaves: the octave down, as written and up.

In Write, a key sets the picked step's note and octave and moves to the next step. Tie and Rest do the same for a held or a silent step, Back and Step move the pick, Accent and Slide switch the picked step's own.

In Transpose, a key shifts the running pattern; the middle C plays it as written, and a right-click on Transpose resets it.

While stopped a key also sounds. The computer's keys Z to comma play the middle octave.

## The sequencer

The patterns 01 to 16, each with an A and a B. Click one to edit it and hear it; while playing, it comes in when the one that plays reaches its end.

Chain: switch it on to play the chain instead. Its blocks are patterns with their repeats. Click a block to edit that pattern, Shift-click or wheel for repeats, right-click to remove it; + adds the pattern you edit. Loop plays the chain round, or once and then stops.

Length, at the pattern row's end: click for 16, 12 or 8 steps, wheel for any.

The grid: drag a step up or down in the pitch lane (or wheel it) for its note; the notes run down the lane's left edge. Below it each step's octave, accent, slide and gate (note, tie or rest). Right-click a step for more.

The motion lane shows the recorded moves of one knob; click its name for the next. Draw them, right-click to clear.

The column beside the grid has two tabs. Line: Generate (a new line in the key), Mutate (four steps changed; the arrow beside it says what may change), the key (root and scale) and Snap to key, Move (the pattern a step earlier or later), Pitch (every note a semitone). Locks: turn a knob there to hold its value on the picked step.

Undo, Redo and Drag MIDI sit at the sequencer's top right; Undo and Redo are also Ctrl+Z and Ctrl+Shift+Z. Drag MIDI: drag it into your music software and the edited pattern goes along as a MIDI file (ties held, a slide as an overlap, accents at full velocity); click it to save the file instead.

## Mods and the Devil Fish

Mods opens the trimmers inside the case over the synth's sections: TM3 (how far the cutoff reaches), TM4 (the tune), TM5 (the width of an octave) and C11 (the square's shaper), as fitted to a real unit. Stock sets them back.

Beside them, the Devil Fish: Robin Whittle's modification of the 303, switched on or off (off is the stock 303, untouched). On, it adds Overdrive, Slide Time, Soft Attack, Normal and Accent Decay, Filter Tracking, Filter FM, the Sweep Speed, Accent Sweep and Muffler switches and the red Accent button (every note accented while held). Cut Off and Env Mod reach further, the bass is fuller, and the panel's Decay then sets the volume's decay (held notes past half). Its Stock sets the knobs where it sounds most like a 303. Devil Fish is Whittle's name; this is a model of his mod from his manual, not his product.

## Under the lid

Lid shows the whole voice as one schematic sheet, every part, the current on its wires as moving dots and their voltage as a green or red tint, live from the circuit as it plays. Wheel to zoom, drag to pan, click a part to read it. Slow replay holds the last two seconds and walks them fifty times slower. Escape or Back to the panel closes it.

## The library

The Library key opens a shelf of lines and sounds: 30 original lines in ten styles and 12 sounds, four of them Devil Fish ones. Audition plays a line in place of the pattern you edit, or your pattern with a sound; Load puts it in, as one undo step; Keep a copy puts one among yours (kept in `%APPDATA%\CoolSynth303\Library`), which you can rename or send to the Recycle Bin.

## MIDI and sync

While stopped, a MIDI note plays the voice (the middle C at MIDI note 36); while playing, a note transposes the pattern from middle C (note 60).

The beat sync plays the 303 in time with CoolSynth 808 and other apps and DAWs that have it, on this computer or the network, in the standalone app (in a DAW the DAW's transport leads). It is on from the start. SYNC beside the tempo lights while it's on and counts the others ("SYNC 2"); a click switches it. With others, the tempo is shared (the top bar shows SYNC) and a Play starts on their next bar line. Play and Stop go to all of them, unless **Options > MIDI > Share Play and Stop** is off.

For hardware it follows an incoming MIDI clock (its start, stop, continue, song position and tempo, the top bar then showing CLOCK) and sends its own while it plays, also while the beat sync leads. While the beat sync has others with it, an incoming clock is ignored.

The 303 and the 808 together take some two and a half processor cores; a busy program beside them can make the 808 stutter.

In a DAW the host's transport runs the sequencer: it plays while the host plays, from where it is, at its tempo (the top bar shows HOST), and a loop in the host starts the pattern again. The save file is the host's project.

## Options

Options opens the 303's own options: the audio device and the accuracy, the MIDI ports, the beat sync's Play and Stop and Claude's MCP server, the window's size and theme, and the WAV export. In a DAW, the Host page has the accuracy.

Accuracy: Best, or Light: the voice at the output's own rate and solved a little less tightly, some 40 % less CPU, the top octave up to 3.5 dB duller. A DAW's export always renders at Best.

The standalone app renders its sound 50 ms ahead of the sound card, so a heavy moment (a note's attack, a knob turned fast) never drops out; keys, knobs and Play answer 50 ms later. The latency shown on the Audio page includes it, and its CPU figure is the rendering's. If the computer still falls behind at Best, the app goes to Light by itself (the note that plays ends there) and shows LIGHT in place of DSP; after a calm stretch it goes back to Best.

Export: the pattern shown or the chain, 1 to 16 times, cut, rung out or as a seamless loop, the mix or the dry voice beside it too, at the app's rate or 96 kHz; a 24-bit WAV rendered at Best while the 303 plays on.

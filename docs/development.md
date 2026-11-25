# CoolSynth 303 development

## How the sound is made

The voice is the engine's TB-303 circuit (`external/coolsynth-engine/src/synth/v3/Tb303Voice.h`): the oscillator's exponential converter and its capacitor's reset, the square's one-transistor shaper, the diode ladder with its drive and read-out, the envelope generator and the accent's circuit, the BA662 VCA, and the output stage (VOLUME into the two-transistor mixer, out at the OUTPUT jack into a recorder's 16k), each as its transistors. The mixer can pull its output down only through a 10k resistor, so with Volume high the loudest notes clip on that side, as Din Sync's recordings of a real unit do.

It runs at twice the output's rate (96 kHz at 48 kHz; at the output's own in Light) and is matched to ngspice renders of the service notes' circuit within a fraction of a cent and about half a decibel. The sequencer plays it as the original's CPU does: six clocks a step, the gate on for three of them, a slide holding it into the next note, accent and slide latched at the step's start, the gate 2 ms behind the clock.

The Devil Fish is the same circuit with the parts Whittle changed set to his values, a current into the filter's control for tracking and FM, and a sustain on the volume envelope. Soft Attack and the Muffler, whose circuits he never published, are simple stand-ins after the VCA, ahead of Volume.

The output ends in a soft saturation: untouched up to -3 dBFS, above it rounded off toward full scale, so a pushed Devil Fish never clips digitally.

## The MCP server

The standalone app can serve Claude over MCP on 127.0.0.1:47633 (`src/mcp`, as CoolSynth's): read and set the panel, run the sequencer, hold keys, record the output into a WAV file, picture the window. It is off until switched on in **Options > MIDI > Claude (MCP server)**, and the switch starts or stops it at once; only this computer can reach it. The switch is kept in `%APPDATA%\CoolSynth303\CoolSynth303.settings` as `<VALUE name="mcpServer" val="1"/>`, so a test setup can switch it on there before the app starts. `COOLSYNTH303_NO_MCP` in the environment keeps it off whatever the switch says. The plugin opens no port.

## Tools

`tools/listening-pass` plays the stock voice through Din Sync's 25 recorded sets of a real TB-303 (each knob from 25 to 100 %, the others at five settings) and serves a page that switches between the real unit and ours while both play. The recordings are not in the repository; they come from dinsync.info.

`tools/library/make_library.py` writes the built-in lines and sounds.

`tools/icon` draws the app's icon at every size.

## Building a release

The Debug build is compiled optimised (the engine's `cmake/CoolSynthFlags.cmake`). The app is `build/CoolSynth303_artefacts/Debug/Standalone/CoolSynth 303.exe`; the plugin is `build/CoolSynth303_artefacts/Debug/VST3/CoolSynth 303.vst3` (copy it to `C:\Program Files\Common Files\VST3`).

A Release build carries the C++ runtime inside it. `scripts/ci/BuildAndTest.ps1 -Configuration Release -RunTests`, then `scripts/ci/PackageRelease.ps1 -Configuration Release -Installer`, build, test and package as the release does (the installer needs Inno Setup 6.5 or newer, `installer/CoolSynth303.iss`). A version tag (`v0.1.0`) makes GitHub build and publish a release.

`THIRD_PARTY_NOTICES.txt` is written by `tools/release/make_notices.py`: the notices of JUCE's image, font and audio libraries, the VST3 SDK, Open303 and the fonts.

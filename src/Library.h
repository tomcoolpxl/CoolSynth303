#pragma once

#include <vector>

#include <juce_core/juce_core.h>

#include "Pattern.h"

/*
    CoolSynth 303's library (CoolSynth's D338): lines and sounds on separate shelves. The built-in ones are written
    by tools/library/make_library.py into resources/library/library303.json and compiled in; they are read-only. Your
    own are kept one file each under %APPDATA%\CoolSynth303\Library (Lines\*.json, Sounds\*.json), and can be renamed
    or sent to the Recycle Bin.

    A line is a pattern with its style, tempo, a word about it and the sound it was written for. A sound is a set of
    panel values by the names the project's JSON gives them; loading it sets those and leaves the rest (the volume,
    the tuning, the trimmers) as they are.
*/
namespace cs303
{
    struct LibraryLine
    {
        juce::String name, style, about, sound;
        double bpm = 125.0;
        Pattern pattern;
        bool builtIn = true;
        juce::File file; // one of yours
    };

    struct LibrarySound
    {
        juce::String name, family, about;
        juce::var values; // panel values by name
        bool builtIn = true;
        juce::File file;
    };

    struct Library
    {
        std::vector<LibraryLine> lines;
        std::vector<LibrarySound> sounds;
    };

    /** The built-in lines and sounds, in the file's order. */
    [[nodiscard]] Library builtInLibrary();
    /** A library from its JSON text (the compiled-in file's, or a test's). */
    [[nodiscard]] Library libraryFromJson(const juce::String& json);

    /** Where your own are kept; a test may point it elsewhere. */
    [[nodiscard]] juce::File userLibraryFolder();
    void setUserLibraryFolderForTesting(const juce::File& folder);
    /** Your own lines and sounds, by name. */
    [[nodiscard]] Library userLibrary();
    /** Keeps a line or a sound of yours under its name (a number added if the name is taken); the file, or none. */
    juce::File saveUserLine(LibraryLine line);
    juce::File saveUserSound(LibrarySound sound);
    /** A new name for one of yours, in the file and as the file's name; the file, or none. */
    juce::File renameUserEntry(const juce::File& file, const juce::String& newName);
    /** One of yours to the Recycle Bin. */
    bool recycleUserEntry(const juce::File& file);

    /** A sound's values set onto a panel; what it does not name stays. */
    [[nodiscard]] Panel applySound(const Panel& panel, const juce::var& values);
    /** A panel as a sound's values (all of the voice's, not the volume, the tuning or the trimmers). */
    [[nodiscard]] juce::var soundValuesOf(const Panel& panel);
}

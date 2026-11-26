cmake_minimum_required(VERSION 3.22)

# macOS as CoolSynth's (its D234, D370): Apple silicon only, macOS 13 Ventura or newer. Set before project(), so every
# target takes them.
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Apple silicon only (CoolSynth's D234)")
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "The oldest macOS it runs on (CoolSynth's D234)")

project(CoolSynth303 VERSION 0.2.1 LANGUAGES C CXX) # C as well: JUCE 9 builds zlib, libpng, libjpeg and libflac as C

# The settings every CoolSynth program shares (the C++ runtime, a Debug build compiled optimised), from the engine.
include(external/coolsynth-engine/cmake/CoolSynthFlags.cmake)

# A Release build optimised across all its code at link time (whole-program optimisation: /GL with /LTCG on MSVC), the
# engine's and JUCE's too, as CoolSynth 808's (its D327). Not AVX2: CoolSynth's own test found it no faster and bit for
# bit the same, and it would shut out every PC from before 2013. The everyday Debug build is not touched. A build may
# pass -DCMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE=OFF to time it without.
if(NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

add_subdirectory(external/JUCE)
# CLAP beside the VST3 (CoolSynth's D367, as CoolSynth's own): free-audio/clap-juce-extensions, pinned in external/ at
# CoolSynth's commit.
add_subdirectory(external/clap-juce-extensions EXCLUDE_FROM_ALL)
include(CTest)
# The engine's own tests run in its repository's CI; here only this app's.
set(COOLSYNTH_ENGINE_TESTS OFF CACHE BOOL "Build the engine's own test program" FORCE)
add_subdirectory(external/coolsynth-engine)

# The panel's typefaces, as CoolSynth 808's: Archivo for every label, IBM Plex Mono for numbers; both SIL OFL, their
# licences beside them in resources/fonts.
juce_add_binary_data(CoolSynth303Data
    HEADER_NAME AppData.h
    NAMESPACE AppData
    SOURCES
        resources/fonts/ArchivoCondensed-SemiBold.ttf
        resources/fonts/ArchivoSemiCondensed-SemiBold.ttf
        resources/fonts/ArchivoSemiCondensed-Bold.ttf
        resources/fonts/Archivo-Medium.ttf
        resources/fonts/Archivo-ExtraBold.ttf
        resources/fonts/IBMPlexMono-Medium.ttf
        resources/fonts/IBMPlexMono-SemiBold.ttf
        resources/lid/tb303-sheet.json
        resources/library/library303.json)

# A JUCE audio plugin: the standalone app first, as CoolSynth 808 (D267), and a VST3 for a DAW (CoolSynth's D296, D305).
juce_add_plugin(CoolSynth303
    COMPANY_NAME "tomcoolpxl"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/coolsynth303-icon-512.png" # the user's pick (D363)
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/coolsynth303-icon-32.png" # the knob alone, for 32 px and under
    PRODUCT_NAME "CoolSynth 303"
    PLUGIN_NAME "CoolSynth 303"
    PLUGIN_MANUFACTURER_CODE Tcpx
    PLUGIN_CODE Cs33
    FORMATS Standalone VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    # macOS asks before an app talks to the local network (the beat sync finds the others by multicast): the reason it
    # shows (CoolSynth's D370).
    PLIST_TO_MERGE "<plist><dict><key>NSLocalNetworkUsageDescription</key><string>CoolSynth 303 plays in time with other music apps on your network through the beat sync.</string></dict></plist>")

# The CLAP version (CoolSynth's D367), as CoolSynth's without its remote control pages. "stereo" is CLAP's audio
# capability (its stereo output), not the voicing. Stepped parameters keep their steps (DISCRETE).
clap_juce_extensions_plugin(TARGET CoolSynth303
    CLAP_ID "com.tomcoolpxl.coolsynth303"
    CLAP_FEATURES instrument synthesizer stereo
    # No "#readme" anchor: a '#' in a define is dropped by the Ninja and Makefile generators (the repository's page
    # opens on the README anyway).
    CLAP_MANUAL_URL "https://github.com/tomcoolpxl/CoolSynth303"
    CLAP_SUPPORT_URL "https://github.com/tomcoolpxl/CoolSynth303/issues"
    CLAP_USE_JUCE_PARAMETER_RANGES DISCRETE)

set(COOLSYNTH303_SOURCES
    src/Pattern.cpp
    src/Sequencer.cpp
    src/Processor.cpp
    src/MidiExport.cpp
    src/Editor.cpp
    src/ui/Theme.cpp
    src/ui/Widgets.cpp
    src/ui/Panel.cpp
    src/ui/Keyboard.cpp
    src/ui/SequencerView.cpp
    src/ui/LidView.cpp
    src/mcp/McpServer.cpp
    src/mcp/Tools303.cpp
    src/Library.cpp
    src/ui/LibraryShelf.cpp
    src/StandaloneDevices.cpp
    src/ui/WindowLook.cpp
    src/ui/OptionsWindow.cpp
    src/AudioExport.cpp)

# The standalone app's own window (D349) only in the app, not in the tests.
target_sources(CoolSynth303 PRIVATE ${COOLSYNTH303_SOURCES} src/StandaloneApp.cpp)
target_include_directories(CoolSynth303 PRIVATE src)
target_compile_features(CoolSynth303 PUBLIC cxx_std_20)
target_compile_definitions(CoolSynth303
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
    PRIVATE
        # Our own app around the standalone (D349, as CoolSynth 808's D311): JUCE's window without its Options
        # button, in the 303's look.
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
target_link_libraries(CoolSynth303
    PRIVATE
        coolsynth_engine
        juce::juce_audio_utils
        juce::juce_gui_extra
        CoolSynth303Data
        coolsynth_config_flags
        juce::juce_recommended_warning_flags)

if(BUILD_TESTING)
    juce_add_console_app(CoolSynth303Tests)
    target_sources(CoolSynth303Tests
        PRIVATE
            tests/TestMain.cpp
    tests/ClockFollowerTests.cpp
            tests/PatternTests.cpp
            tests/SequencerTests.cpp
            tests/ProcessorTests.cpp
            tests/EditorTests.cpp
            tests/HostingTests.cpp
            tests/McpTests.cpp
            tests/LibraryTests.cpp
            tests/OptionsTests.cpp
            tests/ExportTests.cpp
            tests/BeatSyncTests.cpp
            ${COOLSYNTH303_SOURCES})
    target_include_directories(CoolSynth303Tests PRIVATE src tests)
    target_compile_features(CoolSynth303Tests PUBLIC cxx_std_20)
    target_compile_definitions(CoolSynth303Tests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_STANDALONE_APPLICATION=1
            COOLSYNTH303_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            COOLSYNTH303_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
            COOLSYNTH303_CONFIG="$<CONFIG>" # the hosting test loads the VST3 of its own configuration
            JUCE_PLUGINHOST_VST3=1)
    target_link_libraries(CoolSynth303Tests
        PRIVATE
            coolsynth_engine
            juce::juce_audio_utils
            juce::juce_gui_extra
            CoolSynth303Data
            coolsynth_config_flags
            juce::juce_recommended_warning_flags)
    add_test(NAME CoolSynth303Tests COMMAND CoolSynth303Tests)
endif()

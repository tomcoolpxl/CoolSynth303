cmake_minimum_required(VERSION 3.22)

project(CoolSynth303 VERSION 0.1.0 LANGUAGES C CXX) # C as well: JUCE 9 builds zlib, libpng, libjpeg and libflac as C

# The settings every CoolSynth program shares (the C++ runtime, a Debug build compiled optimised), from the engine.
include(external/coolsynth-engine/cmake/CoolSynthFlags.cmake)

add_subdirectory(external/JUCE)
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
        resources/fonts/IBMPlexMono-SemiBold.ttf)

# A JUCE audio plugin: the standalone app first, as CoolSynth 808 (D267), and a VST3 for a DAW (CoolSynth's D296, D305).
juce_add_plugin(CoolSynth303
    COMPANY_NAME "tomcoolpxl"
    PRODUCT_NAME "CoolSynth 303"
    PLUGIN_NAME "CoolSynth 303"
    PLUGIN_MANUFACTURER_CODE Tcpx
    PLUGIN_CODE Cs33
    FORMATS Standalone VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE)

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
    src/ui/SequencerView.cpp)

target_sources(CoolSynth303 PRIVATE ${COOLSYNTH303_SOURCES})
target_include_directories(CoolSynth303 PRIVATE src)
target_compile_features(CoolSynth303 PUBLIC cxx_std_20)
target_compile_definitions(CoolSynth303
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
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
            tests/PatternTests.cpp
            tests/SequencerTests.cpp
            tests/ProcessorTests.cpp
            tests/EditorTests.cpp
            tests/HostingTests.cpp
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

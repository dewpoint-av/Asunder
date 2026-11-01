cmake_minimum_required(VERSION 3.21)
project(Asunder VERSION 1.2.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable VST2 compatibility to prevent conflicts
add_definitions(-DJUCE_VST3_CAN_REPLACE_VST2=0)
add_definitions(-DJUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)

# ---- Vendored dependencies (copied into the tree, no network needed) ------
# JUCE 8.0.8
add_subdirectory(JUCE)

# clap-juce-extensions (bundles the CLAP SDK under clap-libs/)
add_subdirectory(clap-juce-extensions)

# ---- The plugin ------------------------------------------------------------
# VST3 everywhere; on macOS also build an Audio Unit (required for Logic /
# GarageBand, and the common format for Ableton Live on Mac).
set(ASUNDER_PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND ASUNDER_PLUGIN_FORMATS AU)
endif()

juce_add_plugin(Asunder
    COMPANY_NAME "Dewpoint AV"
    COMPANY_WEBSITE "https://dewpoint.live"
    COMPANY_EMAIL "anthony@dewpoint.live"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Dwpt
    PLUGIN_CODE Asun
    FORMATS ${ASUNDER_PLUGIN_FORMATS}
    PRODUCT_NAME "Asunder"
    BUNDLE_ID "com.dewpoint.Asunder"
)

# Add the CLAP format on top of the JUCE-generated VST3 target.
clap_juce_extensions_plugin(TARGET Asunder
    CLAP_ID "live.dewpoint.asunder"
    CLAP_FEATURES audio-effect distortion stereo)

# Generate JuceHeader.h and per-target config
juce_generate_juce_header(Asunder)

# Embedded Monument Extended logotype fonts.
juce_add_binary_data(AsunderBinaryData
    SOURCES
        Source/MonumentExtendedUltrabold.otf
        Source/MonumentExtendedRegular.otf)

# ---- Source files ----------------------------------------------------------
target_sources(Asunder
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/WaveshapeVisualizer.cpp
        Source/TapeVisualizer.cpp
        Source/dsp/DistortionEngine.cpp
        Source/dsp/TapeEngine.cpp
)

target_include_directories(Asunder
    PRIVATE
        Source
)

target_compile_definitions(Asunder
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# ---- Link JUCE modules -----------------------------------------------------
target_link_libraries(Asunder
    PRIVATE
        AsunderBinaryData
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_data_structures
        juce::juce_events
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# ---- Compiler flags --------------------------------------------------------
if(MSVC)
    target_compile_options(Asunder PRIVATE /W4)
else()
    target_compile_options(Asunder PRIVATE -Wall -Wextra)
endif()

# ---- Packaging / staging (Windows) -----------------------------------------
if(WIN32)
    # Stage a flat copy of both formats into build/dist/ for the installer.
    add_custom_command(TARGET Asunder_CLAP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Asunder_CLAP>"
            "${CMAKE_BINARY_DIR}/dist/Asunder.clap"
        COMMENT "Staging Asunder.clap")

    add_custom_command(TARGET Asunder_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/Asunder_artefacts/$<CONFIG>/VST3/Asunder.vst3"
            "${CMAKE_BINARY_DIR}/dist/Asunder.vst3"
        COMMENT "Staging Asunder.vst3")
endif()

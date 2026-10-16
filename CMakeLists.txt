# // BUILD CONFIGURATION
cmake_minimum_required(VERSION 3.22)
project(AuraPitchPro VERSION 4.5.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.3
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AuraPitchPro
    COMPANY_NAME "Angel Music DSP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Amds
    PLUGIN_CODE Aupt
    FORMATS VST3 Standalone
    ProductName "AuraPitch Pro"
)

juce_generate_juce_header(AuraPitchPro)

target_sources(AuraPitchPro
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/NaturalPitchEngine.h
        Source/PluginProcessor.h
        Source/PluginEditor.h
)

target_compile_definitions(AuraPitchPro
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AuraPitchPro
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

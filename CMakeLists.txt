cmake_minimum_required(VERSION 3.22)
project(DaliGTR VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# URL the plugin downloads its Sound Set from on first run (set by CI)
set(DALIGTR_SOUNDSET_URL "" CACHE STRING "Sound Set zip URL")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(DaliGTR
    COMPANY_NAME "Dali Audio"
    BUNDLE_ID com.daliaudio.daligtr
    PLUGIN_MANUFACTURER_CODE DliA
    PLUGIN_CODE Dgtr
    FORMATS VST3 Standalone
    PRODUCT_NAME "DaliGTR"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument)

target_sources(DaliGTR PRIVATE
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.cpp)

target_include_directories(DaliGTR PRIVATE Source)

target_compile_definitions(DaliGTR PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    DALIGTR_SOUNDSET_URL="${DALIGTR_SOUNDSET_URL}")

target_link_libraries(DaliGTR
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

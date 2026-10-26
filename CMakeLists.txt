cmake_minimum_required(VERSION 3.22)
project(BOOMHYPE_VOCAL_RACK VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE lives in ./JUCE (setup.sh adds it as a git submodule / clone)
add_subdirectory(JUCE)

juce_add_plugin(BoomhypeVocalRack
    COMPANY_NAME "BoomHype Tech"
    PLUGIN_MANUFACTURER_CODE Bhyp
    PLUGIN_CODE Bvrk
    FORMATS VST3 Standalone
    PRODUCT_NAME "BOOMHYPE Vocal Rack"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(BoomhypeVocalRack PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BoomhypeVocalRack PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BoomhypeVocalRack
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

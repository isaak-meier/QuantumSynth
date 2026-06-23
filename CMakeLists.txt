cmake_minimum_required(VERSION 3.22)
project(QuantumSynth VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(QuantumSynth
    COMPANY_NAME "Isaak Meier"
    BUNDLE_ID com.isaakmeier.quantumsynth
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Isak
    PLUGIN_CODE Qsyn
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "QuantumSynth")

juce_generate_juce_header(QuantumSynth)

target_sources(QuantumSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_features(QuantumSynth PRIVATE cxx_std_17)

target_compile_definitions(QuantumSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(QuantumSynth PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

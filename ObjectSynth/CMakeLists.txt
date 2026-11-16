cmake_minimum_required(VERSION 3.22)

project(ObjectSynth VERSION 0.1.0 LANGUAGES C CXX)

# Use one fixed, reproducible JUCE release. First CMake configure needs Git/internet.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.5
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ObjectSynth
    COMPANY_NAME "ObjectSynthProject"
    BUNDLE_ID "com.objectsynthproject.objectsynth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE OsyP
    PLUGIN_CODE Osyn
    FORMATS Standalone VST3
    COPY_PLUGIN_AFTER_BUILD FALSE
    PRODUCT_NAME "ObjectSynth"
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE
)

target_sources(ObjectSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Synth/WavetableOscillator.cpp
    Source/Synth/SynthVoice.cpp
    Source/Synth/SynthVoice.h
    Source/Synth/SynthSound.h
    Source/Synth/WavetableLoader.cpp
    Source/DSP/FilterBank.h
)

target_compile_features(ObjectSynth PRIVATE cxx_std_17)

target_compile_definitions(ObjectSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ObjectSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

juce_add_console_app(ObjectSynthWavetableTest
    PRODUCT_NAME "ObjectSynthWavetableTest"
    NEEDS_CURL FALSE
)

target_sources(ObjectSynthWavetableTest PRIVATE
    Tests/WavetableLoaderSmokeTest.cpp
    Source/Synth/WavetableLoader.cpp
)

target_link_libraries(ObjectSynthWavetableTest PRIVATE
    juce::juce_audio_formats
)
# Engine regression tests: compiles the real processor/voice sources into a
# console app and renders audio offline. Run with: ctest --preset debug
juce_add_console_app(ObjectSynthEngineTests
    PRODUCT_NAME "ObjectSynthEngineTests"
    NEEDS_CURL FALSE
)

target_sources(ObjectSynthEngineTests PRIVATE
    Tests/EngineRegressionTests.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Synth/WavetableOscillator.cpp
    Source/Synth/SynthVoice.cpp
    Source/Synth/WavetableLoader.cpp
)

target_compile_features(ObjectSynthEngineTests PRIVATE cxx_std_17)

target_compile_definitions(ObjectSynthEngineTests PRIVATE
    JucePlugin_Name="ObjectSynth"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(ObjectSynthEngineTests PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_audio_formats
)

enable_testing()
add_test(NAME EngineRegressionTests COMMAND ObjectSynthEngineTests)

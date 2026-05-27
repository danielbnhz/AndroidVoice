cmake_minimum_required(VERSION 3.22)
project(AndroidVoice VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

# Download JUCE if not present
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AndroidVoice
    COMPANY_NAME                "nth_tech"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    Ntht
    PLUGIN_CODE                 Avox
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "AndroidVoice"
)

target_sources(AndroidVoice PRIVATE
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/WavetableOscillator.h
    src/AndroidVoice.h
)

target_compile_definitions(AndroidVoice PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AndroidVoice PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
target_include_directories(AndroidVoice PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)
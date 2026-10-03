cmake_minimum_required(VERSION 3.22)
project(mazo VERSION 0.5.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Local builds can point at an existing JUCE checkout; GitHub fetches it.
set(MAZO_JUCE_DIR "" CACHE PATH "Optional local JUCE folder")
if(MAZO_JUCE_DIR)
    add_subdirectory(${MAZO_JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(mazo
    COMPANY_NAME "ZOONIDO"
    PLUGIN_MANUFACTURER_CODE Zoon
    PLUGIN_CODE Mazo
    PRODUCT_NAME "MAZO"
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Drum
    COPY_PLUGIN_AFTER_BUILD FALSE)

# The mockup's fonts, built into the plugin (SIL Open Font License, see resources/fonts)
juce_add_binary_data(mazo_fonts SOURCES
    resources/fonts/Archivo-Regular.ttf
    resources/fonts/Archivo-SemiBold.ttf
    resources/fonts/Archivo-ExtraBold.ttf
    resources/fonts/JetBrainsMono-Regular.ttf
    resources/fonts/JetBrainsMono-SemiBold.ttf)

target_sources(mazo PRIVATE
    source/plugin_processor.cpp
    source/plugin_editor.cpp
    source/preset_manager.cpp)

target_compile_definitions(mazo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(mazo
    PRIVATE
        mazo_fonts
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

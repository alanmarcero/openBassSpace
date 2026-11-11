cmake_minimum_required(VERSION 3.22)

project(openBassSpace VERSION 2.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

option(OBS_BUILD_PLUGIN "Build the plugin" ON)
option(OBS_BUILD_TESTS "Build the unit tests" ON)

include(FetchContent)

add_library(open_bass_space_core STATIC
    Source/Core/BiquadFilter.cpp
    Source/Core/LinkwitzRiley.cpp
    Source/Core/StereoFieldShaper.cpp
)
target_include_directories(open_bass_space_core PUBLIC Source/Core)
set_target_properties(open_bass_space_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(OBS_BUILD_PLUGIN)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)

    juce_add_binary_data(OpenMasterAssets
        SOURCES
            assets/analog-cyber-faceplate.png
            assets/bass-space-nebula.png)
    set_target_properties(OpenMasterAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

    juce_add_plugin(openBassSpace
        COMPANY_NAME "Alan Marcero"
        BUNDLE_ID "com.alanmarcero.openbassspace"
        PRODUCT_NAME "openBassSpace"
        PLUGIN_MANUFACTURER_CODE Amrc
        PLUGIN_CODE Obsp
        FORMATS VST3 AU Standalone
        VST3_CATEGORIES Fx Spatial
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(openBassSpace PRIVATE
        Source/Plugin/Parameters.cpp
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp)

    target_include_directories(openBassSpace PRIVATE
        Source/Core
        Source/Plugin)

    target_compile_definitions(openBassSpace PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(openBassSpace
        PRIVATE
            open_bass_space_core
            OpenMasterAssets
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    add_executable(open_bass_space_capture Tests/PluginHostChecks.cpp)
    target_include_directories(open_bass_space_capture PRIVATE Source/Plugin Source/Core)
    target_link_libraries(open_bass_space_capture PRIVATE openBassSpace juce::juce_audio_utils)
    target_compile_definitions(open_bass_space_capture PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
endif()

if(OBS_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.9.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)

    add_executable(open_bass_space_tests
        Tests/BiquadFilterTests.cpp
        Tests/StereoFieldShaperTests.cpp)

    target_link_libraries(open_bass_space_tests PRIVATE open_bass_space_core Catch2::Catch2WithMain)

    enable_testing()
    include(Catch)
    catch_discover_tests(open_bass_space_tests)
endif()

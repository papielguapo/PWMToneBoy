cmake_minimum_required(VERSION 3.22)
project(PWMToneBoy VERSION 1.5.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(JUCE 8 CONFIG QUIET)
if(NOT JUCE_FOUND)
    include(FetchContent)
    FetchContent_Declare(JUCE URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.tar.gz DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()
juce_add_plugin(PWMToneBoy
    COMPANY_NAME "PAPIELGUAPO"
    BUNDLE_ID com.audiodevices.pwmtoneboy
    PLUGIN_MANUFACTURER_CODE AuDv
    PLUGIN_CODE Pwtb
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics Distortion
    FORMATS VST3 Standalone
    PRODUCT_NAME "PWM TONE BOY")
target_sources(PWMToneBoy PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
set_property(SOURCE Source/PluginProcessor.cpp Source/PluginEditor.cpp Tests/ProcessorChecks.cpp
    APPEND PROPERTY COMPILE_OPTIONS $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_definitions(PWMToneBoy PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(PWMToneBoy PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_basics juce::juce_gui_extra juce::juce_recommended_config_flags)
if(MSVC)
    target_compile_options(PWMToneBoy PRIVATE /W4 /MP4 $<$<CONFIG:Release>:/O2> $<$<CONFIG:Release>:/fp:fast>)
else()
    target_compile_options(PWMToneBoy PRIVATE -Wall -Wextra $<$<CONFIG:Release>:-O3> $<$<CONFIG:Release>:-ffp-contract=fast>)
endif()
option(PWM_BUILD_TESTS "Build offline DSP and editor regression checks" ON)
if(PWM_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(PWMToneBoyChecks PRODUCT_NAME "PWM Tone Boy Checks")
    target_sources(PWMToneBoyChecks PRIVATE Tests/ProcessorChecks.cpp)
    target_link_libraries(PWMToneBoyChecks PRIVATE PWMToneBoy)
    target_include_directories(PWMToneBoyChecks PRIVATE $<TARGET_PROPERTY:PWMToneBoy,INCLUDE_DIRECTORIES>)
    target_compile_definitions(PWMToneBoyChecks PRIVATE $<TARGET_PROPERTY:PWMToneBoy,COMPILE_DEFINITIONS>)
    add_test(NAME PWMToneBoyChecks COMMAND PWMToneBoyChecks)
endif()

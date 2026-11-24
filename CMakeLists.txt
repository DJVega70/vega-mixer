cmake_minimum_required(VERSION 3.22)
project(VegaMixer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static MSVC runtime so the .exe runs without installing the VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_gui_app(VegaMixer
    PRODUCT_NAME "Vega Productions Mixer"
    COMPANY_NAME "Vega Productions"
    VERSION 1.0.0)

juce_generate_juce_header(VegaMixer)

juce_add_binary_data(VegaMixerData SOURCES
    Resources/BarlowCondensed-SemiBold.ttf
    Resources/ShareTechMono-Regular.ttf)

target_sources(VegaMixer PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MixerEngine.cpp)

target_compile_definitions(VegaMixer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=1
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:VegaMixer,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:VegaMixer,JUCE_VERSION>")

target_link_libraries(VegaMixer
    PRIVATE
        VegaMixerData
        juce::juce_gui_extra
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(VEGA_BUILD_TESTS "Build the headless engine test" OFF)
if(VEGA_BUILD_TESTS)
    juce_add_console_app(VegaEngineTest PRODUCT_NAME "VegaEngineTest")
    juce_generate_juce_header(VegaEngineTest)
    target_sources(VegaEngineTest PRIVATE Tests/EngineTest.cpp Source/MixerEngine.cpp)
    target_include_directories(VegaEngineTest PRIVATE Source)
    target_compile_definitions(VegaEngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_FLAC=1)
    target_link_libraries(VegaEngineTest PRIVATE juce::juce_audio_utils juce::juce_gui_extra PUBLIC juce::juce_recommended_config_flags)
endif()

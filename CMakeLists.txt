cmake_minimum_required(VERSION 3.22)

project(WavySuite VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: static C++ runtime, so the plugins load on any PC without a redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# macOS universal builds: -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.16-security
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)
# A tag can be moved: build only from the commit it pointed to when chosen
# (unless FETCHCONTENT_SOURCE_DIR_JUCE points at a JUCE of your own).
set(WAVY_JUCE_COMMIT ea77444750de45e036e0279bb0291144bf8e3688)
if(NOT FETCHCONTENT_SOURCE_DIR_JUCE)
    find_package(Git REQUIRED)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD WORKING_DIRECTORY ${juce_SOURCE_DIR}
        OUTPUT_VARIABLE juce_commit OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT juce_commit STREQUAL WAVY_JUCE_COMMIT)
        message(FATAL_ERROR "JUCE 8.0.16-security is at ${juce_commit}, expected ${WAVY_JUCE_COMMIT}")
    endif()
endif()

# CLAP through clap-juce-extensions (MIT; brings the CLAP SDK).
option(WAVY_CLAP "Build CLAP plugins" ON)
if(WAVY_CLAP)
    FetchContent_Declare(clap_juce_extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d
        GIT_SUBMODULES_RECURSE ON)
    FetchContent_MakeAvailable(clap_juce_extensions)
endif()

# Shared code (toolbar, look) used by every plugin.
add_library(WavyCommon STATIC
    Source/common/WavyToolbar.cpp
    Source/common/WavyRenUI.cpp
    Source/common/WavyFlatUI.cpp
    Source/common/WavyHybridUI.cpp)
target_include_directories(WavyCommon PUBLIC Source/common)
set_target_properties(WavyCommon PROPERTIES POSITION_INDEPENDENT_CODE ON)   # linked into shared plugins (Linux)
target_compile_definitions(WavyCommon PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(WavyCommon PUBLIC
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_dsp
    PRIVATE juce::juce_recommended_config_flags)

# wavy_add_plugin(<Target> <Product name> <4-char code> <clap id> [CATEGORIES <VST3 categories>] SOURCES ...)
function(wavy_add_plugin target product code clapid)
    cmake_parse_arguments(P "" "" "SOURCES;CATEGORIES" ${ARGN})
    if(NOT P_CATEGORIES)
        set(P_CATEGORIES Fx Dynamics)
    endif()
    set(formats VST3)
    if(APPLE)
        list(APPEND formats AU)
    endif()
    juce_add_plugin(${target}
        COMPANY_NAME "Wavy Suite"
        PLUGIN_MANUFACTURER_CODE Wavy
        PLUGIN_CODE ${code}
        FORMATS ${formats}
        PRODUCT_NAME "${product}"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE   # the host keeps its transport shortcuts
        VST3_CATEGORIES ${P_CATEGORIES}
        AU_MAIN_TYPE kAudioUnitType_Effect)
    target_sources(${target} PRIVATE ${P_SOURCES})
    target_link_libraries(${target} PRIVATE WavyCommon juce::juce_recommended_warning_flags)
    if(WAVY_CLAP)
        clap_juce_extensions_plugin(TARGET ${target}
            CLAP_ID "${clapid}" CLAP_FEATURES audio-effect)
    endif()
    # Rebuilds land in build/plugins (an FL Studio VST3 scan folder points here).
    add_custom_command(TARGET ${target}_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_FILE_DIR:${target}_VST3>/../.."
            "${CMAKE_BINARY_DIR}/plugins/${product}.vst3")
endfunction()

enable_testing()
add_subdirectory(plugins/WVox)
add_subdirectory(plugins/WBass)
add_subdirectory(plugins/WBassX)
add_subdirectory(plugins/WComp)
add_subdirectory(plugins/WDelay)
add_subdirectory(plugins/WBassRider)
add_subdirectory(plugins/WVoxRider)
add_subdirectory(plugins/WVoxChain)

# Offline renderer of the WVox DSP (null test, tuning). No JUCE needed.
add_executable(WVoxRender tools/WVoxRender.cpp)
target_include_directories(WVoxRender PRIVATE plugins/WVox)

# Unit tests (DSP + processor):  cmake --build build --target WVoxTests, then ctest --test-dir build -C Release
juce_add_console_app(WVoxTests PRODUCT_NAME "WVoxTests")
target_sources(WVoxTests PRIVATE
    tests/WVoxTests.cpp
    plugins/WVox/WVoxProcessor.cpp
    plugins/WVox/WVoxEditor.cpp)
target_include_directories(WVoxTests PRIVATE plugins/WVox)
target_compile_definitions(WVoxTests PRIVATE
    JucePlugin_Name="WVox" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WVoxTests PRIVATE WavyCommon juce::juce_recommended_config_flags)
add_test(NAME WVoxTests COMMAND WVoxTests)

# Windows-only check that the plugins leave the keyboard to the host (tools/FocusCheck.cpp)
if(WIN32)
    juce_add_console_app(FocusCheck PRODUCT_NAME "FocusCheck")
    target_sources(FocusCheck PRIVATE tools/FocusCheck.cpp)
    target_compile_definitions(FocusCheck PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(FocusCheck PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)

    # records a plug-in editor's meters while audio plays (meter ballistics of the references; tools/MeterCapture.cpp)
    juce_add_gui_app(MeterCapture PRODUCT_NAME "MeterCapture")
    target_sources(MeterCapture PRIVATE tools/MeterCapture.cpp)
    target_compile_definitions(MeterCapture PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(MeterCapture PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)

    # renders WComp's VU dial alone at a given needle angle, for the comparison with the filmed reference (tools/VuRender.cpp)
    juce_add_console_app(VuRender PRODUCT_NAME "VuRender")
    target_sources(VuRender PRIVATE tools/VuRender.cpp)
    target_link_libraries(VuRender PRIVATE WavyCommon juce::juce_recommended_config_flags)

    # renders a reference offline with its side-chain bus fed and a host tempo (tools/RefRender.cpp)
    juce_add_console_app(RefRender PRODUCT_NAME "RefRender")
    target_sources(RefRender PRIVATE tools/RefRender.cpp)
    target_compile_definitions(RefRender PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(RefRender PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)

    # decodes Ogg Vorbis to raw float32, for test material from Ogg-coded samples (tools/OggDecode.cpp)
    juce_add_console_app(OggDecode PRODUCT_NAME "OggDecode")
    target_sources(OggDecode PRIVATE tools/OggDecode.cpp)
    target_compile_definitions(OggDecode PRIVATE JUCE_USE_OGGVORBIS=1)
    target_link_libraries(OggDecode PRIVATE juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()

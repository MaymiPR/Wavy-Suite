wavy_add_plugin(WBassX "WBassX" Wbx1 "com.wavysuite.wbassx"
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/WBassXProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/WBassXEditor.cpp)
target_include_directories(WBassX PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Editor snapshot tool (renders the GUI to a PNG for the visual comparison).
juce_add_console_app(WBassXSnapshot PRODUCT_NAME "WBassXSnapshot")
target_sources(WBassXSnapshot PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/Snapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassXProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassXEditor.cpp)
target_include_directories(WBassXSnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WBassXSnapshot PRIVATE
    JucePlugin_Name="WBassX" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WBassXSnapshot PRIVATE WavyCommon juce::juce_recommended_config_flags)

# Offline renderer of the DSP for the null test (nulltest/nulltest_wbassx.py).
add_executable(WBassXRender ${CMAKE_SOURCE_DIR}/tools/WBassXRender.cpp)
target_include_directories(WBassXRender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Unit tests (DSP + processor):  cmake --build build --target WBassXTests, then ctest --test-dir build -C Release
juce_add_console_app(WBassXTests PRODUCT_NAME "WBassXTests")
target_sources(WBassXTests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/WBassXTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassXProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassXEditor.cpp)
target_include_directories(WBassXTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WBassXTests PRIVATE
    JucePlugin_Name="WBassX" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WBassXTests PRIVATE WavyCommon juce::juce_recommended_config_flags)
add_test(NAME WBassXTests COMMAND WBassXTests)

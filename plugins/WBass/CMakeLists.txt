wavy_add_plugin(WBass "WBass" Wbs1 "com.wavysuite.wbass"
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/WBassProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/WBassEditor.cpp)
target_include_directories(WBass PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Editor snapshot tool (renders the GUI to a PNG for the visual comparison).
juce_add_console_app(WBassSnapshot PRODUCT_NAME "WBassSnapshot")
target_sources(WBassSnapshot PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/Snapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassEditor.cpp)
target_include_directories(WBassSnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WBassSnapshot PRIVATE
    JucePlugin_Name="WBass" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WBassSnapshot PRIVATE WavyCommon juce::juce_recommended_config_flags)

# Offline renderer of the WBass DSP (null tests and fitting scripts)
add_executable(WBassRender ${CMAKE_SOURCE_DIR}/tools/WBassRender.cpp)
target_include_directories(WBassRender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Unit tests (DSP + processor):  cmake --build build --target WBassTests, then ctest --test-dir build -C Release
juce_add_console_app(WBassTests PRODUCT_NAME "WBassTests")
target_sources(WBassTests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/WBassTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassEditor.cpp)
target_include_directories(WBassTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WBassTests PRIVATE
    JucePlugin_Name="WBass" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WBassTests PRIVATE WavyCommon juce::juce_recommended_config_flags)
add_test(NAME WBassTests COMMAND WBassTests)

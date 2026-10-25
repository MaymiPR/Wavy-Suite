wavy_add_plugin(WDelay "WDelay" Wdl1 "com.wavysuite.wdelay"
    CATEGORIES Fx Delay
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/WDelayProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/WDelayEditor.cpp)
target_include_directories(WDelay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/plugins/WComp)

# Offline renderer for the null tests (the DSP only)
add_executable(WDelayRender ${CMAKE_SOURCE_DIR}/tools/WDelayRender.cpp)
target_include_directories(WDelayRender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/plugins/WComp)

# Editor snapshot tool (renders the GUI to a PNG for the visual comparison).
juce_add_console_app(WDelaySnapshot PRODUCT_NAME "WDelaySnapshot")
target_sources(WDelaySnapshot PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/Snapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WDelayProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WDelayEditor.cpp)
target_include_directories(WDelaySnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/plugins/WComp)
target_compile_definitions(WDelaySnapshot PRIVATE
    JucePlugin_Name="WDelay" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WDelaySnapshot PRIVATE WavyCommon juce::juce_recommended_config_flags)

# Unit tests (parameters, processor, DSP):  cmake --build build --target WDelayTests, then ctest --test-dir build -C Release
juce_add_console_app(WDelayTests PRODUCT_NAME "WDelayTests")
target_sources(WDelayTests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/WDelayTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WDelayProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WDelayEditor.cpp)
target_include_directories(WDelayTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/plugins/WComp)
target_compile_definitions(WDelayTests PRIVATE
    JucePlugin_Name="WDelay" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WDelayTests PRIVATE WavyCommon juce::juce_recommended_config_flags)
add_test(NAME WDelayTests COMMAND WDelayTests)

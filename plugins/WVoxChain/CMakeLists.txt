wavy_add_plugin(WVoxChain "WVox Chain" Wvc1 "com.wavysuite.wvoxchain"
    CATEGORIES Fx
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainEditor.cpp)
target_include_directories(WVoxChain PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Editor snapshot tool (renders the GUI to a PNG for the visual comparison).
juce_add_console_app(WVoxChainSnapshot PRODUCT_NAME "WVoxChainSnapshot")
target_sources(WVoxChainSnapshot PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/Snapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainEditor.cpp)
target_include_directories(WVoxChainSnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WVoxChainSnapshot PRIVATE
    JucePlugin_Name="WVox Chain" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WVoxChainSnapshot PRIVATE WavyCommon juce::juce_recommended_config_flags)

# Unit tests (parameters, processor, editor):  cmake --build build --target WVoxChainTests, then ctest --test-dir build -C Release
juce_add_console_app(WVoxChainTests PRODUCT_NAME "WVoxChainTests")
target_sources(WVoxChainTests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/WVoxChainTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainEditor.cpp)
target_include_directories(WVoxChainTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WVoxChainTests PRIVATE
    JucePlugin_Name="WVox Chain" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WVoxChainTests PRIVATE WavyCommon juce::juce_recommended_config_flags)
add_test(NAME WVoxChainTests COMMAND WVoxChainTests)


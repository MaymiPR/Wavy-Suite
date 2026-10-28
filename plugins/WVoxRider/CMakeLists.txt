wavy_add_plugin(WVoxRider "WVox Rider" Wvr1 "com.wavysuite.wvoxrider"
    CATEGORIES Fx Dynamics
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/WVoxRiderProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/WVoxRiderEditor.cpp)
target_include_directories(WVoxRider PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Editor snapshot tool (renders the GUI to a PNG for the visual comparison).
juce_add_console_app(WVoxRiderSnapshot PRODUCT_NAME "WVoxRiderSnapshot")
target_sources(WVoxRiderSnapshot PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/Snapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxRiderProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxRiderEditor.cpp)
target_include_directories(WVoxRiderSnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WVoxRiderSnapshot PRIVATE
    JucePlugin_Name="WVox Rider" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WVoxRiderSnapshot PRIVATE WavyCommon juce::juce_recommended_config_flags)

# Unit tests (parameters, processor, editor):  cmake --build build --target WVoxRiderTests, then ctest --test-dir build -C Release
juce_add_console_app(WVoxRiderTests PRODUCT_NAME "WVoxRiderTests")
target_sources(WVoxRiderTests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/WVoxRiderTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxRiderProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxRiderEditor.cpp)
target_include_directories(WVoxRiderTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WVoxRiderTests PRIVATE
    JucePlugin_Name="WVox Rider" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WVoxRiderTests PRIVATE WavyCommon juce::juce_recommended_config_flags)
add_test(NAME WVoxRiderTests COMMAND WVoxRiderTests)

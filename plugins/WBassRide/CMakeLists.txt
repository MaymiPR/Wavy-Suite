wavy_add_plugin(WBassRide "WBass Ride" Wbr1 "com.wavysuite.wbassride"
    CATEGORIES Fx Dynamics
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/WBassRideProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/WBassRideEditor.cpp)
target_include_directories(WBassRide PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Editor snapshot tool (renders the GUI to a PNG for the visual comparison).
juce_add_console_app(WBassRideSnapshot PRODUCT_NAME "WBassRideSnapshot")
target_sources(WBassRideSnapshot PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/Snapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassRideProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassRideEditor.cpp)
target_include_directories(WBassRideSnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WBassRideSnapshot PRIVATE
    JucePlugin_Name="WBass Ride" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WBassRideSnapshot PRIVATE WavyCommon juce::juce_recommended_config_flags)

# Unit tests (parameters, processor, editor):  cmake --build build --target WBassRideTests, then ctest --test-dir build -C Release
juce_add_console_app(WBassRideTests PRODUCT_NAME "WBassRideTests")
target_sources(WBassRideTests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/WBassRideTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassRideProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassRideEditor.cpp)
target_include_directories(WBassRideTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WBassRideTests PRIVATE
    JucePlugin_Name="WBass Ride" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WBassRideTests PRIVATE WavyCommon juce::juce_recommended_config_flags)
add_test(NAME WBassRideTests COMMAND WBassRideTests)

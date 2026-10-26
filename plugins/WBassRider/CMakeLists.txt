wavy_add_plugin(WBassRider "WBass Rider" Wbr1 "com.wavysuite.wbassrider"
    CATEGORIES Fx Dynamics
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/WBassRiderProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/WBassRiderEditor.cpp)
target_include_directories(WBassRider PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Editor snapshot tool (renders the GUI to a PNG for the visual comparison).
juce_add_console_app(WBassRiderSnapshot PRODUCT_NAME "WBassRiderSnapshot")
target_sources(WBassRiderSnapshot PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/Snapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassRiderProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassRiderEditor.cpp)
target_include_directories(WBassRiderSnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WBassRiderSnapshot PRIVATE
    JucePlugin_Name="WBass Rider" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WBassRiderSnapshot PRIVATE WavyCommon juce::juce_recommended_config_flags)

# Unit tests (parameters, processor, editor):  cmake --build build --target WBassRiderTests, then ctest --test-dir build -C Release
juce_add_console_app(WBassRiderTests PRODUCT_NAME "WBassRiderTests")
target_sources(WBassRiderTests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/WBassRiderTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassRiderProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassRiderEditor.cpp)
target_include_directories(WBassRiderTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WBassRiderTests PRIVATE
    JucePlugin_Name="WBass Rider" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WBassRiderTests PRIVATE WavyCommon juce::juce_recommended_config_flags)
add_test(NAME WBassRiderTests COMMAND WBassRiderTests)

# Offline renderer for the null tests: the plug-in's processor on raw float files (tools/WBassRiderRender.cpp)
juce_add_console_app(WBassRiderRender PRODUCT_NAME "WBassRiderRender")
target_sources(WBassRiderRender PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/WBassRiderRender.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassRiderProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WBassRiderEditor.cpp)
target_include_directories(WBassRiderRender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WBassRiderRender PRIVATE
    JucePlugin_Name="WBass Rider" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WBassRiderRender PRIVATE WavyCommon juce::juce_recommended_config_flags)

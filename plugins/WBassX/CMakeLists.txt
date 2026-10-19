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

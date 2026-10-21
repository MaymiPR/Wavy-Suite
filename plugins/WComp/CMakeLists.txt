wavy_add_plugin(WComp "WComp" Wcp1 "com.wavysuite.wcomp"
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/WCompProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/WCompEditor.cpp)
target_include_directories(WComp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Editor snapshot tool (renders the GUI to a PNG for the visual comparison).
juce_add_console_app(WCompSnapshot PRODUCT_NAME "WCompSnapshot")
target_sources(WCompSnapshot PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/Snapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WCompProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WCompEditor.cpp)
target_include_directories(WCompSnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WCompSnapshot PRIVATE
    JucePlugin_Name="WComp" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WCompSnapshot PRIVATE WavyCommon juce::juce_recommended_config_flags)

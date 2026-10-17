wavy_add_plugin(WVox "WVox" Wvx1 "com.wavysuite.wvox"
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/WVoxProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/WVoxEditor.cpp)
target_include_directories(WVox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Editor snapshot tool (renders the GUI to a PNG for the visual comparison).
juce_add_console_app(WVoxSnapshot PRODUCT_NAME "WVoxSnapshot")
target_sources(WVoxSnapshot PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/WVoxSnapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxEditor.cpp)
target_include_directories(WVoxSnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WVoxSnapshot PRIVATE
    JucePlugin_Name="WVox" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WVoxSnapshot PRIVATE WavyCommon juce::juce_recommended_config_flags)

# the reverb's measured responses per rate (nulltest/pack_voxchain_reverb.py)
juce_add_binary_data(WVoxChainIR HEADER_NAME WVoxChainIR.h NAMESPACE WVoxChainIR SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ir/reverb_44100.bin ${CMAKE_CURRENT_SOURCE_DIR}/ir/reverb_48000.bin
    ${CMAKE_CURRENT_SOURCE_DIR}/ir/reverb_88200.bin ${CMAKE_CURRENT_SOURCE_DIR}/ir/reverb_96000.bin)
set_target_properties(WVoxChainIR PROPERTIES POSITION_INDEPENDENT_CODE ON)

wavy_add_plugin(WVoxChain "WVox Chain" Wvc1 "com.wavysuite.wvoxchain"
    CATEGORIES Fx
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainDsp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainEditor.cpp)
target_include_directories(WVoxChain PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/plugins/WDelay ${CMAKE_SOURCE_DIR}/plugins/WComp)
target_link_libraries(WVoxChain PRIVATE WVoxChainIR)

# Editor snapshot tool (renders the GUI to a PNG for the visual comparison).
juce_add_console_app(WVoxChainSnapshot PRODUCT_NAME "WVoxChainSnapshot")
target_sources(WVoxChainSnapshot PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/Snapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainDsp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainEditor.cpp)
target_include_directories(WVoxChainSnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/plugins/WDelay ${CMAKE_SOURCE_DIR}/plugins/WComp)
target_compile_definitions(WVoxChainSnapshot PRIVATE
    JucePlugin_Name="WVox Chain" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WVoxChainSnapshot PRIVATE WavyCommon WVoxChainIR juce::juce_recommended_config_flags)

# Unit tests (parameters, processor, editor):  cmake --build build --target WVoxChainTests, then ctest --test-dir build -C Release
juce_add_console_app(WVoxChainTests PRODUCT_NAME "WVoxChainTests")
target_sources(WVoxChainTests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/WVoxChainTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainDsp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainEditor.cpp)
target_include_directories(WVoxChainTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/plugins/WDelay ${CMAKE_SOURCE_DIR}/plugins/WComp)
target_compile_definitions(WVoxChainTests PRIVATE
    JucePlugin_Name="WVox Chain" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WVoxChainTests PRIVATE WavyCommon WVoxChainIR juce::juce_recommended_config_flags)
add_test(NAME WVoxChainTests COMMAND WVoxChainTests)


# Offline renderer for the null tests: the plug-in's processor on raw float files (tools/WVoxChainRender.cpp)
juce_add_console_app(WVoxChainRender PRODUCT_NAME "WVoxChainRender")
target_sources(WVoxChainRender PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/WVoxChainRender.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainDsp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WVoxChainEditor.cpp)
target_include_directories(WVoxChainRender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/plugins/WDelay ${CMAKE_SOURCE_DIR}/plugins/WComp)
target_compile_definitions(WVoxChainRender PRIVATE
    JucePlugin_Name="WVox Chain" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
target_link_libraries(WVoxChainRender PRIVATE WavyCommon WVoxChainIR juce::juce_recommended_config_flags)

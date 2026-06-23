cmake_minimum_required(VERSION 3.22)

project(SuperVelocityCurve VERSION 0.2.27)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Post-build install to ~/Library/Audio/Plug-Ins is for local dev only — slows/hangs CI.
if(DEFINED ENV{CI})
    set(SVC_COPY_PLUGIN_AFTER_BUILD FALSE)
else()
    set(SVC_COPY_PLUGIN_AFTER_BUILD TRUE)
endif()

# LTO makes Apple Clang CI links take 60–90+ minutes for dual JUCE plugins. Keep for local Release.
option(SVC_ENABLE_LTO "Link-time optimization (disable in CI)" ON)
if(DEFINED ENV{CI})
    set(SVC_ENABLE_LTO OFF CACHE BOOL "LTO off in CI" FORCE)
endif()

add_subdirectory(JUCE)

juce_add_binary_data(SuperVelocityCurveBinaryData
    SOURCES
        Resources/pluginval_badge.png
        Resources/developer_portrait.png)

set(SVC_PLUGIN_SOURCES
    Source/Engine/VelocityCurve.cpp
    Source/Engine/VelocityEngine.cpp
    Source/Engine/MidiVelocityTransport.cpp
    Source/Engine/MidiUtilities.cpp
    Source/Profiles/ControllerProfile.cpp
    Source/Profiles/ProfileStore.cpp
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.cpp
    Source/Standalone/StandaloneMidiPanel.cpp
    Source/UI/PadGridComponent.cpp
    Source/UI/CurveEditorComponent.cpp
    Source/UI/PadInspectorComponent.cpp
    Source/UI/HistogramComponent.cpp
    Source/UI/CalibrationWizardComponent.cpp
    Source/UI/MidiRoutingPanel.cpp
    Source/UI/NoteRemapEditorComponent.cpp
    Source/UI/MidiActivityMeterComponent.cpp
    Source/UI/AboutPanelComponent.cpp
    Source/UI/UnsavedProfileDialogComponent.cpp
    Source/UI/CollapsibleSection.cpp
    Source/UI/HistogramPairPanel.cpp
    Source/UI/EditorLayout.cpp)

function(svc_configure_plugin_target target)
    juce_generate_juce_header(${target})

    target_sources(${target} PRIVATE ${SVC_PLUGIN_SOURCES})

    target_compile_definitions(${target}
        PRIVATE
            SVC_VERSION="${PROJECT_VERSION}")

    target_compile_definitions(${target}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_include_directories(${target} PRIVATE
        ${CMAKE_BINARY_DIR}/juce_binarydata_SuperVelocityCurveBinaryData/JuceLibraryCode)

    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            SuperVelocityCurveBinaryData
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    if(SVC_ENABLE_LTO)
        target_link_libraries(${target} PUBLIC juce::juce_recommended_lto_flags)
    endif()
endfunction()

# Linker-signed JUCE bundles fail Ableton's PluginScanner ("code has no resources").
function(svc_adhoc_sign_plugin_bundles)
    if(NOT APPLE)
        return()
    endif()
    set(_argv ${ARGN})
    list(LENGTH _argv _argc)
    math(EXPR _end "${_argc} - 2")
    foreach(_i RANGE 0 ${_end} 2)
        list(GET _argv ${_i} bundle_target)
        math(EXPR _j "${_i} + 1")
        list(GET _argv ${_j} installed_path)
        if(NOT TARGET ${bundle_target})
            continue()
        endif()
        add_custom_command(TARGET ${bundle_target} POST_BUILD
            COMMAND xattr -cr "$<TARGET_BUNDLE_DIR:${bundle_target}>"
            COMMAND dot_clean -m "$<TARGET_BUNDLE_DIR:${bundle_target}>" || true
            COMMAND codesign --force --sign - --timestamp=none --deep "$<TARGET_BUNDLE_DIR:${bundle_target}>"
            COMMENT "Ad-hoc sign ${bundle_target} build bundle")
        if(SVC_COPY_PLUGIN_AFTER_BUILD)
            add_custom_command(TARGET ${bundle_target} POST_BUILD
                COMMAND xattr -cr "${installed_path}"
                COMMAND dot_clean -m "${installed_path}" || true
                COMMAND codesign --force --sign - --timestamp=none --deep "${installed_path}"
                COMMENT "Ad-hoc sign ${bundle_target} installed copy")
        endif()
    endforeach()
endfunction()

# Instrument VST3 + AU Music Device (aumu) + Standalone — Ableton instrument/MIDI track slot.
juce_add_plugin(SuperVelocityCurve
    COMPANY_NAME "Super Velocity Curve"
    PLUGIN_MANUFACTURER_CODE Svvc
    PLUGIN_CODE Svcv
    BUNDLE_ID com.supervelocitycurve.instrument
    ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/Resources/AppIcon.png
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Super Velocity Curve"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${SVC_COPY_PLUGIN_AFTER_BUILD})

svc_configure_plugin_target(SuperVelocityCurve)

if (APPLE AND TARGET SuperVelocityCurve_Standalone)
    add_custom_command (TARGET SuperVelocityCurve_Standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E touch "$<TARGET_BUNDLE_DIR:SuperVelocityCurve_Standalone>"
        COMMENT "Refresh Standalone .app bundle timestamp")
endif()

# MIDI FX VST3 + AU MIDI Processor (aumi) — Logic MIDI FX slot, Reaper, Bitwig.
juce_add_plugin(SuperVelocityCurveMidiFx
    COMPANY_NAME "Super Velocity Curve"
    PLUGIN_MANUFACTURER_CODE Svvc
    PLUGIN_CODE Svc2
    BUNDLE_ID com.supervelocitycurve.midifx
    ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/Resources/AppIcon.png
    FORMATS AU VST3
    PRODUCT_NAME "Super Velocity Curve MIDI FX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    VST3_CATEGORIES Fx
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${SVC_COPY_PLUGIN_AFTER_BUILD})

svc_configure_plugin_target(SuperVelocityCurveMidiFx)

if(APPLE)
    svc_adhoc_sign_plugin_bundles(
        SuperVelocityCurve_VST3 "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/Super Velocity Curve.vst3"
        SuperVelocityCurve_AU "$ENV{HOME}/Library/Audio/Plug-Ins/Components/Super Velocity Curve.component"
        SuperVelocityCurveMidiFx_VST3 "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/Super Velocity Curve MIDI FX.vst3"
        SuperVelocityCurveMidiFx_AU "$ENV{HOME}/Library/Audio/Plug-Ins/Components/Super Velocity Curve MIDI FX.component")
endif()

option(SVC_BUILD_CLAP "Build CLAP format via clap-juce-extensions" ON)

if(SVC_BUILD_CLAP AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/clap-juce-extensions/CMakeLists.txt")
    add_subdirectory(third_party/clap-juce-extensions EXCLUDE_FROM_ALL)

    clap_juce_extensions_plugin(TARGET SuperVelocityCurveMidiFx
        CLAP_ID "com.supervelocitycurve.midifx"
        CLAP_FEATURES note-effect)

    if(SVC_COPY_PLUGIN_AFTER_BUILD AND APPLE)
        add_custom_command(TARGET SuperVelocityCurveMidiFx_CLAP POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP/Super Velocity Curve MIDI FX.clap"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<TARGET_BUNDLE_DIR:SuperVelocityCurveMidiFx_CLAP>"
                "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP/Super Velocity Curve MIDI FX.clap"
            COMMAND codesign --force --sign - --timestamp=none --deep
                "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP/Super Velocity Curve MIDI FX.clap"
            COMMENT "Install CLAP to ~/Library/Audio/Plug-Ins/CLAP/")
    endif()
endif()

option(SVC_BUILD_TESTS "Build engine unit tests" ON)

if(SVC_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(SuperVelocityCurveTests
        PRODUCT_NAME "SuperVelocityCurveTests")

    juce_generate_juce_header(SuperVelocityCurveTests)

    target_sources(SuperVelocityCurveTests
        PRIVATE
            Tests/EngineTests.cpp
            Source/Engine/VelocityCurve.cpp
            Source/Engine/VelocityEngine.cpp
    Source/Engine/MidiVelocityTransport.cpp
            Source/Engine/MidiUtilities.cpp
            Source/Profiles/ControllerProfile.cpp
            Source/Profiles/ProfileStore.cpp)

    target_include_directories(SuperVelocityCurveTests PRIVATE Source)

    target_link_libraries(SuperVelocityCurveTests PRIVATE
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_data_structures
        juce::juce_recommended_warning_flags)

    add_test(NAME EngineTests COMMAND SuperVelocityCurveTests)

    juce_add_console_app(SuperVelocityCurveLayoutTests
        PRODUCT_NAME "SuperVelocityCurveLayoutTests")

    juce_generate_juce_header(SuperVelocityCurveLayoutTests)

    target_sources(SuperVelocityCurveLayoutTests
        PRIVATE
            Tests/LayoutTests.cpp
            ${SVC_PLUGIN_SOURCES})

    target_include_directories(SuperVelocityCurveLayoutTests PRIVATE Source)

    target_compile_definitions(SuperVelocityCurveLayoutTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JucePlugin_Name="Super Velocity Curve"
            JucePlugin_Manufacturer="Super Velocity Curve"
            JucePlugin_ManufacturerCode=0x53767663
            JucePlugin_PluginCode=0x53766376
            JucePlugin_IsSynth=0
            JucePlugin_IsMidiEffect=0
            JucePlugin_WantsMidiInput=1
            JucePlugin_ProducesMidiOutput=1)

    target_link_libraries(SuperVelocityCurveLayoutTests PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_basics
        juce::juce_gui_extra
        SuperVelocityCurveBinaryData
        juce::juce_recommended_warning_flags)

    add_test(NAME LayoutTests COMMAND SuperVelocityCurveLayoutTests)
endif()

option(SVC_EXPORT_FACTORY_PROFILES "Build factory profile export tool" ON)

if(SVC_EXPORT_FACTORY_PROFILES)
    juce_add_console_app(ExportFactoryProfiles
        PRODUCT_NAME "ExportFactoryProfiles")

    juce_generate_juce_header(ExportFactoryProfiles)

    target_sources(ExportFactoryProfiles
        PRIVATE
            Tools/ExportFactoryProfiles.cpp
            Source/Profiles/ControllerProfile.cpp
            Source/Engine/VelocityCurve.cpp
            Source/Engine/VelocityEngine.cpp
    Source/Engine/MidiVelocityTransport.cpp
            Source/Engine/MidiUtilities.cpp)

    target_include_directories(ExportFactoryProfiles PRIVATE Source)

    target_link_libraries(ExportFactoryProfiles PRIVATE
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_data_structures
        juce::juce_recommended_warning_flags)

    add_custom_target(export-factory-profiles
        COMMAND ExportFactoryProfiles
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ExportFactoryProfiles
        COMMENT "Export factory .svcp profile files")
endif()

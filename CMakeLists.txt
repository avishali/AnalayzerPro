cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# BUILD CONFIGURATION
# ==============================================================================

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
    if(IOS)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version" FORCE)
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version" FORCE)
    endif()
endif()

option(UniversalBinary "Build universal binary for macOS" OFF)
if(APPLE AND UniversalBinary AND NOT IOS)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE INTERNAL "")
endif()

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ==============================================================================
# PLUGIN CONFIGURATION
# ==============================================================================

set(PLUGIN_NAME "AnalyzerPro" CACHE STRING "Plugin name")
set(PLUGIN_VERSION_MAJOR 1 CACHE STRING "Plugin version major")
set(PLUGIN_VERSION_MINOR 0 CACHE STRING "Plugin version minor")
set(PLUGIN_VERSION_PATCH 0 CACHE STRING "Plugin version patch")
set(COMPANY_NAME "MelecDSP" CACHE STRING "Company name")
set(PLUGIN_CODE "AnPr" CACHE STRING "4-character plugin code (e.g., Tmpl)")
set(MANUFACTURER_CODE "Melc" CACHE STRING "4-character manufacturer code (e.g., Tmpl) - must have at least one uppercase letter")
set(PRODUCT_NAME "${PLUGIN_NAME}" CACHE STRING "Product display name")

set(IS_SYNTH FALSE CACHE BOOL "Is this a synthesizer?")
set(NEEDS_MIDI_INPUT FALSE CACHE BOOL "Does the plugin need MIDI input?")
set(NEEDS_MIDI_OUTPUT FALSE CACHE BOOL "Does the plugin need MIDI output?")
set(IS_MIDI_EFFECT FALSE CACHE BOOL "Is this a MIDI effect?")
set(EDITOR_WANTS_KEYBOARD_FOCUS FALSE CACHE BOOL "Does the editor need keyboard focus?")

# ==============================================================================
# DEV MODE (define BEFORE first use)
# ==============================================================================

option(PLUGIN_DEV_MODE "Fast iteration dev mode (limits formats, disables LTO)" ON)

if(PLUGIN_DEV_MODE)
    set(PLUGIN_DEV_MODE_VALUE 1)
    message(STATUS "Build mode: DEV (fast iteration)")
else()
    set(PLUGIN_DEV_MODE_VALUE 0)
    message(STATUS "Build mode: RELEASE")
endif()

# Plugin formats to build
if(PLUGIN_DEV_MODE)
    set(PLUGIN_FORMATS "VST3;Standalone" CACHE STRING "Plugin formats to build (AU, VST3, AAX, Standalone)" FORCE)
else()
    set(PLUGIN_FORMATS "AU;VST3;Standalone" CACHE STRING "Plugin formats to build (AU, VST3, AAX, Standalone)" FORCE)
endif()

# ==============================================================================
# SDK PATHS
# ==============================================================================

if(NOT DEFINED JUCE_PATH)
    if(DEFINED ENV{JUCE_PATH})
        set(JUCE_PATH "$ENV{JUCE_PATH}")
    else()
        message(FATAL_ERROR "JUCE_PATH not set. Please set it via -DJUCE_PATH=/path/to/JUCE or environment variable.")
    endif()
endif()

get_filename_component(JUCE_PATH "${JUCE_PATH}" ABSOLUTE)

if(NOT EXISTS "${JUCE_PATH}")
    message(FATAL_ERROR "JUCE path not found: ${JUCE_PATH}\nPlease set JUCE_PATH env var or -DJUCE_PATH=/path/to/JUCE")
endif()

# ==============================================================================
# PROJECT SETUP
# ==============================================================================

project(${PLUGIN_NAME}
    VERSION ${PLUGIN_VERSION_MAJOR}.${PLUGIN_VERSION_MINOR}.${PLUGIN_VERSION_PATCH}
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

option(PLUGIN_EDITOR_RESIZABLE "Enable mouse-resizable plugin editor" OFF)

# Disable unity builds (best for incremental iteration)
set(JUCE_BUILD_UNITY_PLUGIN OFF CACHE BOOL "Disable unity build for plugin targets" FORCE)

# ==============================================================================
# CMAKE SAFETY ASSERTS (fail early)
# ==============================================================================

# 1) Detect the classic footgun: putting control flow inside target_link_libraries(...)
# This scans THIS CMakeLists.txt for "target_link_libraries(" followed by "if (" before the closing ")"
# It's intentionally conservative; if it ever false-positives, you can loosen the regex.
file(READ "${CMAKE_CURRENT_LIST_FILE}" _root_cmakelists_text)
string(REGEX MATCH "target_link_libraries\\([^\\)]*\\n[^\\)]*\\n[^\\)]*if[ \\t]*\\(" _bad_tll "${_root_cmakelists_text}")
if(_bad_tll)
    message(FATAL_ERROR
        "CMake safety check failed: Detected 'if()' inside a target_link_libraries(...) argument list.\n"
        "Move the if() outside and call target_link_libraries() separately."
    )
endif()

# ==============================================================================
# JUCE
# ==============================================================================

add_subdirectory("${JUCE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

# ==============================================================================
# ui_core (OBJECT library + interface includes)
# ==============================================================================

add_subdirectory(ui_core)
# ==============================================================================
# MelechDSP shared modules (via submodule)
# ==============================================================================

add_subdirectory(
    third_party/melechdsp-hq/shared/mdsp_core
)

add_subdirectory(
    third_party/melechdsp-hq/shared/mdsp_ui
)

add_subdirectory(
    third_party/melechdsp-hq/shared/mdsp_dsp
)

# ==============================================================================
# PLUGIN TARGET
# ==============================================================================

juce_add_plugin(${PLUGIN_NAME}
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "${COMPANY_NAME}"
    IS_SYNTH ${IS_SYNTH}
    NEEDS_MIDI_INPUT ${NEEDS_MIDI_INPUT}
    NEEDS_MIDI_OUTPUT ${NEEDS_MIDI_OUTPUT}
    IS_MIDI_EFFECT ${IS_MIDI_EFFECT}
    EDITOR_WANTS_KEYBOARD_FOCUS ${EDITOR_WANTS_KEYBOARD_FOCUS}
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE ${MANUFACTURER_CODE}
    PLUGIN_CODE ${PLUGIN_CODE}
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "${PRODUCT_NAME}"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "AnalyzerPro needs microphone access to capture audio input devices, including virtual loopback devices such as BlackHole."
)

if(APPLE)
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_NSMicrophoneUsageDescription
        "AnalyzerPro needs microphone access to capture audio input devices, including virtual loopback devices such as BlackHole."
    )
endif()

# Also set globally so it applies to JUCE-generated bundle targets (e.g. Standalone app) in Xcode projects.
if(APPLE)
    set(CMAKE_XCODE_ATTRIBUTE_INFOPLIST_KEY_NSMicrophoneUsageDescription
        "AnalyzerPro needs microphone access to capture audio input devices, including virtual loopback devices such as BlackHole."
    )
endif()

target_sources(${PLUGIN_NAME}
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/parameters/Parameters.cpp
        Source/parameters/Parameters.h
        Source/ui/MainView.cpp
        Source/ui/MainView.h
        Source/ui/meters/MeterComponent.cpp
        Source/ui/meters/MeterComponent.h
        Source/ui/meters/MeterGroupComponent.cpp
        Source/ui/meters/MeterGroupComponent.h
        Source/hardware/PluginHardwareAdapter.cpp
        Source/hardware/PluginHardwareAdapter.h
        Source/hardware/PluginHardwareOutputAdapter.cpp
        Source/hardware/PluginHardwareOutputAdapter.h
        Source/control/ControlSpecs.cpp
        Source/control/ControlBinder.cpp
        Source/control/AnalyzerProParamIdMap.cpp
        Source/control/AnalyzerProControlContext.cpp
        Source/analyzer/AnalyzerEngine.cpp
        Source/ui/analyzer/AnalyzerDisplayView.cpp
        Source/ui/analyzer/rta1_import/RTADisplay.cpp
        Source/ui/layout/HeaderBar.cpp
        Source/ui/layout/ControlRail.cpp
        Source/ui/layout/FooterBar.cpp
        Source/ui/layout/control_primitives/SectionHeader.cpp
        Source/ui/layout/control_primitives/ChoiceRow.cpp
        Source/ui/layout/control_primitives/ToggleRow.cpp
        Source/ui/layout/control_primitives/SliderRow.cpp
        Source/ui/views/AnalyzerGridPlaceholder.cpp
        Source/ui/views/PhaseCorrelationView.cpp
        Source/audio/DeviceRoutingHelper.cpp
        Source/audio/DeviceRoutingHelper.h
        # ui_core OBJECT sources get added below via TARGET_OBJECTS
)

# Pull ui_core's compiled objects into the plugin (fast iteration)
# (ui_core_obj is defined in ui_core/CMakeLists.txt below)
target_sources(${PLUGIN_NAME} PRIVATE $<TARGET_OBJECTS:ui_core_obj>)

# Temporary: make deprecated-declarations an error for RTADisplay.cpp to pinpoint exact callsite
set_source_files_properties(
    Source/ui/analyzer/rta1_import/RTADisplay.cpp
    PROPERTIES
        COMPILE_OPTIONS "-Werror=deprecated-declarations"
)

target_compile_features(${PLUGIN_NAME} PUBLIC cxx_std_17)

target_include_directories(${PLUGIN_NAME}
    PRIVATE
        Source
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/melechdsp-hq/shared/mdsp_ui/include
)

target_compile_definitions(${PLUGIN_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    PRIVATE
        $<$<BOOL:${PLUGIN_EDITOR_RESIZABLE}>:PLUGIN_EDITOR_RESIZABLE=1>
        PLUGIN_DEV_MODE=${PLUGIN_DEV_MODE_VALUE}
)

# Link JUCE modules + ui_core interface target (includes/defs)
target_link_libraries(${PLUGIN_NAME}
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_basics
        juce::juce_dsp
        mdsp_ui
        mdsp_dsp
    PUBLIC
        ui_core
        mdsp_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Link LTO flags only for release builds (dev mode off)
if(NOT PLUGIN_DEV_MODE)
    target_link_libraries(${PLUGIN_NAME} PUBLIC juce::juce_recommended_lto_flags)
endif()

# ==============================================================================
# TARGET GRAPH ASSERTS (fail early / readable)
# ==============================================================================

function(assert_target_not_self_linking tgt)
    if(NOT TARGET "${tgt}")
        message(FATAL_ERROR "Assert failed: target '${tgt}' does not exist yet.")
    endif()

    # Direct link libs
    get_target_property(_ll "${tgt}" LINK_LIBRARIES)
    if(_ll)
        list(FIND _ll "${tgt}" _idx)
        if(NOT _idx EQUAL -1)
            message(FATAL_ERROR "CMake safety check failed: target '${tgt}' LINK_LIBRARIES contains itself.")
        endif()
    endif()

    # Interface link libs
    get_target_property(_ill "${tgt}" INTERFACE_LINK_LIBRARIES)
    if(_ill)
        list(FIND _ill "${tgt}" _iidx)
        if(NOT _iidx EQUAL -1)
            message(FATAL_ERROR "CMake safety check failed: target '${tgt}' INTERFACE_LINK_LIBRARIES contains itself.")
        endif()
    endif()
endfunction()

assert_target_not_self_linking(${PLUGIN_NAME})

# ==============================================================================
# STATUS MESSAGES
# ==============================================================================

message(STATUS "")
message(STATUS "==========================================")
message(STATUS "Plugin Configuration:")
message(STATUS "  Name: ${PLUGIN_NAME}")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Company: ${COMPANY_NAME}")
message(STATUS "  Formats: ${PLUGIN_FORMATS}")
message(STATUS "  Dev Mode: ${PLUGIN_DEV_MODE}")
message(STATUS "")
message(STATUS "SDK Paths:")
message(STATUS "  JUCE: ${JUCE_PATH}")
message(STATUS "==========================================")
message(STATUS "")

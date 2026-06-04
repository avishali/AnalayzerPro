cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# BUILD CONFIGURATION
# ==============================================================================

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(MDSP_CONSUMER_BRIDGE_ENFORCEMENT "WARN" CACHE STRING
    "AnalyzerPro policy mode for mdsp consumer bridge checks")

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
set(PLUGIN_VERSION_MAJOR 1 CACHE STRING "Plugin version major" FORCE)
set(PLUGIN_VERSION_MINOR 1 CACHE STRING "Plugin version minor" FORCE)
set(PLUGIN_VERSION_PATCH 1 CACHE STRING "Plugin version patch" FORCE)
set(COMPANY_NAME "MelechDSP" CACHE STRING "Company name")
set(PLUGIN_CODE "AnPr" CACHE STRING "4-character plugin code (e.g., Tmpl)")
set(MANUFACTURER_CODE "Melc" CACHE STRING "4-character manufacturer code (e.g., Tmpl) - must have at least one uppercase letter")
set(PRODUCT_NAME "${PLUGIN_NAME}" CACHE STRING "Product display name")

set(IS_SYNTH FALSE CACHE BOOL "Is this a synthesizer?")
set(NEEDS_MIDI_INPUT FALSE CACHE BOOL "Does the plugin need MIDI input?")
set(NEEDS_MIDI_OUTPUT FALSE CACHE BOOL "Does the plugin need MIDI output?")
set(IS_MIDI_EFFECT FALSE CACHE BOOL "Is this a MIDI effect?")
set(EDITOR_WANTS_KEYBOARD_FOCUS FALSE CACHE BOOL "Does the editor need keyboard focus?")
set(AAX_SDK_PATH "" CACHE PATH "Path to AAX SDK root (contains Interfaces/ACF)")

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
    set(PLUGIN_FORMATS "VST3;AAX;Standalone" CACHE STRING "Plugin formats to build (AU, VST3, AAX, Standalone)" FORCE)
else()
    set(PLUGIN_FORMATS "AU;VST3;AAX;Standalone" CACHE STRING "Plugin formats to build (AU, VST3, AAX, Standalone)" FORCE)
endif()

option(ANALYZERPRO_COPY_AFTER_BUILD "Copy built plugins to system folders after build" ON)
option(ANALYZERPRO_METAL_EDITOR "Full-editor Metal UI" OFF)

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
# MelechDSP HQ root (main repo path; overrides third_party copy)
# ==============================================================================

set(MELECHDSP_HQ_ROOT "" CACHE PATH "Path to melechdsp-hq; if empty, use third_party copy")

# Resolve HQ_DIR robustly. Support both layouts:
#   A) <HQ>/CMakeLists.txt
#   B) <HQ>/melechdsp-hq/CMakeLists.txt (nested wrapper checkout)
set(_hq_candidates "")

if (MELECHDSP_HQ_ROOT)
    # Normalize to absolute early (handles relative values from presets)
    get_filename_component(_HQ_ABS "${MELECHDSP_HQ_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
    list(APPEND _hq_candidates
        "${_HQ_ABS}"
        "${_HQ_ABS}/melechdsp-hq"
    )
endif()

list(APPEND _hq_candidates
    "${CMAKE_CURRENT_LIST_DIR}/third_party/melechdsp-hq"
    "${CMAKE_CURRENT_LIST_DIR}/third_party/melechdsp-hq/melechdsp-hq"
)

set(HQ_DIR "")
foreach(p IN LISTS _hq_candidates)
    if (EXISTS "${p}/CMakeLists.txt" OR EXISTS "${p}/shared/mdsp_core/CMakeLists.txt")
        get_filename_component(HQ_DIR "${p}" ABSOLUTE)
        break()
    endif()
endforeach()

if (NOT HQ_DIR)
    message(FATAL_ERROR
        "Could not locate melechdsp-hq root. Tried:\n"
        "  ${_hq_candidates}\n"
        "Fix your submodule checkout or set -DMELECHDSP_HQ_ROOT=/path/to/melechdsp-hq"
    )
endif()

message(STATUS "AnalyzerPro: using melechdsp-hq from: ${HQ_DIR}")

# ==============================================================================
# JUCE BinaryBuilder override (optional, auto-detected if available)
# ==============================================================================

# If BinaryBuilder was built via the HQ helper script, set it automatically (only if user hasn't set it)
if (NOT JUCE_BINARYBUILDER_EXE)
    set(_bb "${HQ_DIR}/build_binarybuilder/extras/BinaryBuilder/BinaryBuilder_artefacts/Release/BinaryBuilder")
    if (EXISTS "${_bb}")
        set(JUCE_BINARYBUILDER_EXE "${_bb}" CACHE FILEPATH "" )
        message(STATUS "AnalyzerPro: auto-detected BinaryBuilder: ${JUCE_BINARYBUILDER_EXE}")
    endif()
endif()

# ==============================================================================
# PROJECT SETUP
# ==============================================================================

project(${PLUGIN_NAME}
    VERSION ${PLUGIN_VERSION_MAJOR}.${PLUGIN_VERSION_MINOR}.${PLUGIN_VERSION_PATCH}
    LANGUAGES C CXX
)

if(ANALYZERPRO_METAL_EDITOR)
    if(APPLE)
        enable_language(OBJCXX)
    else()
        message(FATAL_ERROR "ANALYZERPRO_METAL_EDITOR requires macOS / Apple platforms")
    endif()
endif()

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

# Prefer explicit AAX SDK path for reproducible local/CI builds.
# Fallback to env var when cache variable is not provided.
if ((AAX_SDK_PATH STREQUAL "") AND DEFINED ENV{AAX_SDK_PATH})
    set(AAX_SDK_PATH "$ENV{AAX_SDK_PATH}")
endif()

if (NOT AAX_SDK_PATH STREQUAL "")
    juce_set_aax_sdk_path("${AAX_SDK_PATH}")
    message(STATUS "AnalyzerPro: using AAX SDK from: ${AAX_SDK_PATH}")
endif()

# ==============================================================================
# MelechDSP SDK (from HQ_DIR)
# ==============================================================================

# HQ integration:
# - Modern HQ: add_subdirectory(HQ_DIR) provides mdsp::sdk / mdsp_sdk
# - Legacy HQ (no top-level CMakeLists): add the module subdirs and create a local mdsp::sdk aggregate
if (EXISTS "${HQ_DIR}/CMakeLists.txt")
    add_subdirectory("${HQ_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/mdsp_sdk")
else()
    message(STATUS "AnalyzerPro: HQ has no top-level CMakeLists.txt; using legacy module integration")
    add_subdirectory("${HQ_DIR}/shared/mdsp_core" "${CMAKE_CURRENT_BINARY_DIR}/_deps/hq_mdsp_core")
    add_subdirectory("${HQ_DIR}/shared/mdsp_dsp"  "${CMAKE_CURRENT_BINARY_DIR}/_deps/hq_mdsp_dsp")
    add_subdirectory("${HQ_DIR}/shared/mdsp_ui"   "${CMAKE_CURRENT_BINARY_DIR}/_deps/hq_mdsp_ui")
    add_subdirectory("${HQ_DIR}/shared/mdsp_gui"  "${CMAKE_CURRENT_BINARY_DIR}/_deps/hq_mdsp_gui")

    if (NOT TARGET mdsp_sdk)
        add_library(mdsp_sdk INTERFACE)
    endif()
    if (NOT TARGET mdsp::sdk)
        add_library(mdsp::sdk ALIAS mdsp_sdk)
    endif()

    target_link_libraries(mdsp_sdk INTERFACE
        mdsp_core
        mdsp_dsp
        mdsp_ui
        mdsp_gui
    )

    # Ensure include roots are visible even if module targets are mis-scoped in older checkouts
    target_include_directories(mdsp_sdk INTERFACE
        "${HQ_DIR}/shared/mdsp_core/include"
        "${HQ_DIR}/shared/mdsp_dsp/include"
        "${HQ_DIR}/shared/mdsp_ui/include"
        "${HQ_DIR}/shared/mdsp_gui/include"
    )
endif()

# Bridge-consumer policy helper is provided by modern HQ.
# Fallback: include helper directly when available in older/nonstandard checkouts.
if (NOT COMMAND mdsp_enforce_bridge_consumer AND EXISTS "${HQ_DIR}/cmake/mdsp_consumer_bridge.cmake")
    include("${HQ_DIR}/cmake/mdsp_consumer_bridge.cmake")
endif()

# Read MDSP SDK version:
# 1) Prefer MDSP_SDK_VERSION target property (modern HQ)
# 2) Fallback to melechdsp_hq_VERSION (modern HQ project var)
# 3) Fallback to git describe in HQ_DIR (works for submodule checkouts)
set(MDSP_SDK_VERSION "")

if (TARGET mdsp::sdk)
    get_property(MDSP_SDK_VERSION TARGET mdsp::sdk PROPERTY MDSP_SDK_VERSION)
elseif (TARGET mdsp_sdk)
    get_property(MDSP_SDK_VERSION TARGET mdsp_sdk PROPERTY MDSP_SDK_VERSION)
endif()

if (NOT MDSP_SDK_VERSION AND DEFINED melechdsp_hq_VERSION)
    set(MDSP_SDK_VERSION "${melechdsp_hq_VERSION}")
endif()

if (NOT MDSP_SDK_VERSION)
    find_package(Git QUIET)
    if (GIT_FOUND AND EXISTS "${HQ_DIR}/.git")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${HQ_DIR}" describe --tags --abbrev=0
            OUTPUT_VARIABLE _mdsp_tag
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)" _mdsp_m "${_mdsp_tag}")
        if (CMAKE_MATCH_1)
            set(MDSP_SDK_VERSION "${CMAKE_MATCH_1}")
        endif()
    endif()
endif()

if (NOT MDSP_SDK_VERSION)
    set(MDSP_SDK_VERSION "0.0.0")
    message(WARNING "Could not determine MDSP SDK version; defaulting to ${MDSP_SDK_VERSION}")
endif()

message(STATUS "MDSP SDK version: ${MDSP_SDK_VERSION}")

# Enforce minimum version 0.2.2 when a real semver is available
if (NOT MDSP_SDK_VERSION STREQUAL "0.0.0")
    if (MDSP_SDK_VERSION VERSION_LESS "0.2.2")
        message(FATAL_ERROR "MDSP SDK version ${MDSP_SDK_VERSION} is below minimum required version 0.2.2")
    endif()
endif()

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
    COPY_PLUGIN_AFTER_BUILD ${ANALYZERPRO_COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE ${MANUFACTURER_CODE}
    PLUGIN_CODE ${PLUGIN_CODE}
    FORMATS ${PLUGIN_FORMATS}
    AAX_CATEGORY SoundField
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
        Source/ui/ControlPanel.cpp
        Source/ui/ControlPanel.h
        Source/ui/meters/MeterComponent.cpp
        Source/ui/meters/MeterComponent.h
        Source/ui/meters/MeterGroupComponent.cpp
        Source/ui/meters/MeterGroupComponent.h
        Source/ui/meters/StereoScopeComponent.cpp
        Source/ui/meters/StereoScopeComponent.h
        Source/ui/meters/PhaseFanScopeComponent.cpp
        Source/ui/meters/PhaseFanScopeComponent.h
        Source/control/ControlSpecs.cpp
        Source/control/ControlBinder.cpp
        Source/control/AnalyzerProParamIdMap.cpp
        Source/control/AnalyzerProControlContext.cpp
        Source/analyzer/AnalyzerEngine.cpp
        Source/ui/analyzer/AnalyzerDisplayView.cpp
        Source/ui/analyzer/AnalyzerViewModel.cpp
        Source/ui/analyzer/StereoScopeView.cpp
        Source/ui/layout/HeaderBar.cpp
        Source/ui/layout/ControlRail.cpp
        Source/ui/layout/DraggableParamValueLabel.cpp
        Source/ui/layout/FooterBar.cpp
        Source/ui/layout/SettingsPopupPanel.cpp
        Source/ui/theme/TraceColors.cpp
        Source/audio/DeviceRoutingHelper.cpp
        Source/audio/DeviceRoutingHelper.h
        Source/ui/tooltips/TooltipManager.cpp
        Source/ui/tooltips/TooltipOverlayComponent.cpp
        Source/presets/PresetManager.cpp
        Source/presets/ABStateManager.cpp
        Source/ui/loudness/LoudnessNumericPanel.cpp
)

if(APPLE AND ANALYZERPRO_METAL_EDITOR)
    target_sources(${PLUGIN_NAME}
        PRIVATE
            Source/ui/analyzer/metal/MetalHost.h
            Source/ui/analyzer/metal/MetalHost.mm
            Source/ui/analyzer/metal/MetalHostShared.h
            Source/ui/analyzer/metal/IEditorSurface.h
            Source/ui/analyzer/metal/MetalEditorRenderer.h
            Source/ui/analyzer/metal/MetalEditorRenderer.mm
            Source/ui/analyzer/metal/MetalEditorShaders.metal
    )
endif()

# ------------------------------------------------------------------
# Ensure mdsp SDK include dirs/defs propagate to the actual JUCE build targets
# ------------------------------------------------------------------
set(_mdsp_consumer_targets
    ${PLUGIN_NAME}
    ${PLUGIN_NAME}_Standalone
    ${PLUGIN_NAME}_VST3
    ${PLUGIN_NAME}_AAX
    ${PLUGIN_NAME}_SharedCode
)

foreach(t IN LISTS _mdsp_consumer_targets)
    if (TARGET ${t})
        if (TARGET mdsp::sdk)
            target_link_libraries(${t} PRIVATE mdsp::sdk)
        elseif (TARGET mdsp_sdk)
            target_link_libraries(${t} PRIVATE mdsp_sdk)
        else()
            message(FATAL_ERROR "mdsp SDK target not found (expected mdsp::sdk or mdsp_sdk). Check HQ_DIR/add_subdirectory.")
        endif()
        if (COMMAND mdsp_enforce_bridge_consumer)
            mdsp_enforce_bridge_consumer(${t})
        endif()
        target_compile_definitions(${t} PRIVATE MDSP_SDK_VERSION_STRING="${MDSP_SDK_VERSION}")
    endif()
endforeach()

if(APPLE AND ANALYZERPRO_METAL_EDITOR)
    foreach(t IN LISTS _mdsp_consumer_targets)
        if (TARGET ${t})
            target_compile_definitions(${t} PRIVATE ANALYZERPRO_METAL_EDITOR=1)
            target_link_libraries(${t} PRIVATE
                "-framework Metal"
                "-framework QuartzCore"
                "-framework CoreVideo"
                "-framework Foundation"
                "-framework AppKit"
            )
        endif()
    endforeach()
endif()

if (COMMAND mdsp_report_bridge_consumer_summary)
    mdsp_report_bridge_consumer_summary()
endif()

target_compile_features(${PLUGIN_NAME} PUBLIC cxx_std_17)

target_include_directories(${PLUGIN_NAME}
    PRIVATE
        Source
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

# Link JUCE modules
target_link_libraries(${PLUGIN_NAME}
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_basics
        juce::juce_dsp
        $<$<BOOL:$<TARGET_EXISTS:mdsp::sdk>>:mdsp::sdk>
        $<$<AND:$<NOT:$<TARGET_EXISTS:mdsp::sdk>>,$<BOOL:$<TARGET_EXISTS:mdsp_sdk>>>:mdsp_sdk>
    PUBLIC
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

# Experimental AAX: VBlank-driven analyzer spectrum UI tick (AnalyzerDisplayView). Other formats
# ignore this; code is active only when JucePlugin_Build_AAX && ANALYZERPRO_AAX_USE_VBLANK_UI_TICK.
option(ANALYZERPRO_AAX_USE_VBLANK_UI_TICK "Experimental AAX: drive analyzer UI pump from VBlankAttachment" OFF)
if(ANALYZERPRO_AAX_USE_VBLANK_UI_TICK)
    foreach(_vb_t IN ITEMS ${PLUGIN_NAME} ${PLUGIN_NAME}_SharedCode ${PLUGIN_NAME}_AAX)
        if(TARGET "${_vb_t}")
            target_compile_definitions("${_vb_t}" PRIVATE ANALYZERPRO_AAX_USE_VBLANK_UI_TICK=1)
        endif()
    endforeach()
    message(STATUS "AnalyzerPro: ANALYZERPRO_AAX_USE_VBLANK_UI_TICK=ON")
endif()

# Optional PACE wraptool sign + verify after AAX build (distribution signing; macOS only).
# Secrets stay in the environment (never in CMake cache): AAX_WRAPTOOL_PASSWORD, etc.
# See scripts/wraptool_sign_aax.sh
option(ENABLE_AAX_WRAPTOOL_SIGN "Run PACE wraptool on the AAX bundle after each AAX build (macOS)" OFF)
set(AAX_WRAPTOOL "" CACHE FILEPATH "Path to wraptool binary (optional; script defaults to Fusion Current)")

if(APPLE AND ENABLE_AAX_WRAPTOOL_SIGN)
    if("${PLUGIN_FORMATS}" MATCHES "AAX" AND TARGET ${PLUGIN_NAME}_AAX)
        if(AAX_WRAPTOOL)
            set(_aax_wraptool_arg "${AAX_WRAPTOOL}")
        else()
            set(_aax_wraptool_arg "")
        endif()
        add_custom_command(TARGET ${PLUGIN_NAME}_AAX POST_BUILD
            COMMAND /bin/bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/wraptool_sign_aax.sh"
                "$<TARGET_FILE:${PLUGIN_NAME}_AAX>"
                "${_aax_wraptool_arg}"
            COMMENT "PACE wraptool: sign + verify AAX"
            VERBATIM
        )
        message(STATUS "  AAX wraptool POST_BUILD: ON (ENABLE_AAX_WRAPTOOL_SIGN)")
    elseif(ENABLE_AAX_WRAPTOOL_SIGN)
        message(WARNING "ENABLE_AAX_WRAPTOOL_SIGN is ON but AAX is not in PLUGIN_FORMATS or ${PLUGIN_NAME}_AAX target is missing; skipping wraptool step.")
    endif()
endif()

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

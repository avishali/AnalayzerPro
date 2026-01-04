# JUCE Plugin Template CMakeLists.txt
# This template can be used to create new JUCE plugins
#
# To use this template:
# 1. Copy this entire folder to create a new plugin
# 2. Rename the folder to your plugin name
# 3. Update PLUGIN_NAME, COMPANY_NAME, PLUGIN_CODE, and MANUFACTURER_CODE below
# 4. Update the source files in Source/ directory
# 5. Run: cmake -S . -B build -DJUCE_PATH=/path/to/JUCE

cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# BUILD CONFIGURATION
# ==============================================================================

# Compile commands, useful for IDEs like VS Code, CLion, etc.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Minimum macOS deployment target
if(APPLE)
    if(IOS)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version" FORCE)
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version" FORCE)
    endif()
endif()

# Universal binary option (build for both x86_64 and arm64)
option(UniversalBinary "Build universal binary for macOS" OFF)
if(APPLE AND UniversalBinary AND NOT IOS)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE INTERNAL "")
endif()

# Static linking in Windows
if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ==============================================================================
# PLUGIN CONFIGURATION - UPDATE THESE VALUES FOR YOUR PLUGIN
# ==============================================================================
set(PLUGIN_NAME "PluginTemplate" CACHE STRING "Plugin name")
set(PLUGIN_VERSION_MAJOR 1 CACHE STRING "Plugin version major")
set(PLUGIN_VERSION_MINOR 0 CACHE STRING "Plugin version minor")
set(PLUGIN_VERSION_PATCH 0 CACHE STRING "Plugin version patch")
set(COMPANY_NAME "YourCompany" CACHE STRING "Company name")
set(PLUGIN_CODE "Tmpl" CACHE STRING "4-character plugin code (e.g., Tmpl)")
set(MANUFACTURER_CODE "Tmpl" CACHE STRING "4-character manufacturer code (e.g., Tmpl) - must have at least one uppercase letter")
set(PRODUCT_NAME "${PLUGIN_NAME}" CACHE STRING "Product display name")

# Plugin type configuration
set(IS_SYNTH FALSE CACHE BOOL "Is this a synthesizer?")
set(NEEDS_MIDI_INPUT FALSE CACHE BOOL "Does the plugin need MIDI input?")
set(NEEDS_MIDI_OUTPUT FALSE CACHE BOOL "Does the plugin need MIDI output?")
set(IS_MIDI_EFFECT FALSE CACHE BOOL "Is this a MIDI effect?")
set(EDITOR_WANTS_KEYBOARD_FOCUS FALSE CACHE BOOL "Does the editor need keyboard focus?")

# Plugin formats to build
set(PLUGIN_FORMATS "AU;VST3;Standalone" CACHE STRING "Plugin formats to build (AU, VST3, AAX, Standalone)")

# ==============================================================================
# SDK PATHS - These can be set via environment variables or CMake cache
# ==============================================================================

# Default paths (can be overridden via -DJUCE_PATH)
if(NOT DEFINED JUCE_PATH)
    if(DEFINED ENV{JUCE_PATH})
        set(JUCE_PATH "$ENV{JUCE_PATH}")
    else()
        message(FATAL_ERROR "JUCE_PATH not set. Please set it via -DJUCE_PATH=/path/to/JUCE or environment variable.")
    endif()
endif()

# Convert to absolute path
get_filename_component(JUCE_PATH "${JUCE_PATH}" ABSOLUTE)

# Verify path exists
if(NOT EXISTS "${JUCE_PATH}")
    message(FATAL_ERROR "JUCE path not found: ${JUCE_PATH}\n"
            "Please set JUCE_PATH environment variable or -DJUCE_PATH=/path/to/JUCE")
endif()

# ==============================================================================
# PROJECT SETUP
# ==============================================================================

project(${PLUGIN_NAME} VERSION ${PLUGIN_VERSION_MAJOR}.${PLUGIN_VERSION_MINOR}.${PLUGIN_VERSION_PATCH} LANGUAGES C CXX)

# Set C++ standard
# ADD — CMakeLists.txt (root), after project(...)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add CMake module path (for custom find scripts)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# ADD — CMakeLists.txt (root)
option(PLUGIN_EDITOR_RESIZABLE "Enable mouse-resizable plugin editor" OFF)

# Add JUCE
# IMPORTANT: This references JUCE from the SDK path, it does NOT copy it.
# The second parameter "${CMAKE_CURRENT_BINARY_DIR}/JUCE" tells CMake where to
# place build artifacts for JUCE, but the source files remain in the original SDK location.
# EXCLUDE_FROM_ALL prevents JUCE targets from being built unless they're dependencies.
add_subdirectory("${JUCE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

# Add ui_core library
add_subdirectory(ui_core)

# ==============================================================================
# PLUGIN TARGET
# ==============================================================================

# PLUGIN_FORMATS is already a CMake list (semicolon-separated), pass it directly
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
    PRODUCT_NAME "${PRODUCT_NAME}")

# Add source files
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
        Source/hardware/PluginHardwareAdapter.cpp
        Source/hardware/PluginHardwareAdapter.h
        Source/hardware/PluginHardwareOutputAdapter.cpp
        Source/hardware/PluginHardwareOutputAdapter.h
)
# ADD — CMakeLists.txt (root), after juce_add_plugin(...)
target_compile_features(${PLUGIN_NAME} PUBLIC cxx_std_17)

# Include directories
target_include_directories(${PLUGIN_NAME}
    PRIVATE
        Source
)

# Compile definitions
target_compile_definitions(${PLUGIN_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# ADD — CMakeLists.txt (root), after juce_add_plugin() when ${PLUGIN_NAME} exists
target_compile_definitions(${PLUGIN_NAME}
    PRIVATE
        $<$<BOOL:${PLUGIN_EDITOR_RESIZABLE}>:PLUGIN_EDITOR_RESIZABLE=1>
)


# Link JUCE modules
target_link_libraries(${PLUGIN_NAME}
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_basics
        juce::juce_dsp
        ui_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

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
message(STATUS "")
message(STATUS "SDK Paths:")
message(STATUS "  JUCE: ${JUCE_PATH}")
message(STATUS "==========================================")
message(STATUS "")

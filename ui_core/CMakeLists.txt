cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# UI_CORE OBJECT LIBRARY (fast incremental builds)
# ==============================================================================

add_library(ui_core_obj OBJECT
    src/ui_core.cpp
)

target_include_directories(ui_core_obj
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ui_core_obj
    PUBLIC
        cxx_std_17
)

# If ui_core needs JUCE compile defs/includes, attach them here (safe for OBJECT libs too)
# target_link_libraries(ui_core_obj PUBLIC juce::juce_gui_basics)

# ==============================================================================
# UI_CORE INTERFACE TARGET (propagates includes/flags to consumers)
# ==============================================================================

add_library(ui_core INTERFACE)

target_include_directories(ui_core
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ui_core
    INTERFACE
        cxx_std_17
)
cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# UI_CORE STATIC LIBRARY
# ==============================================================================

add_library(ui_core STATIC
    src/ui_core.cpp
)

target_include_directories(ui_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ui_core
    PUBLIC
        cxx_std_17
)

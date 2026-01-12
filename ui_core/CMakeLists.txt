# ==============================================================================
# UI CORE LIBRARY
# ==============================================================================

# 1. Object library for compilation (speed)
add_library(ui_core_obj OBJECT
    UiCore.cpp
    UiCore.h
)

target_include_directories(ui_core_obj
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(ui_core_obj PUBLIC cxx_std_17)

# 2. Interface library for linking (usage)
add_library(ui_core INTERFACE)

target_include_directories(ui_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_sources(ui_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/UiCore.h
)

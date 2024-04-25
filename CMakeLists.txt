cmake_minimum_required (VERSION 3.20)

project (NativeMenuDemo
    VERSION 0.1.0
    LANGUAGES C CXX
)

add_subdirectory (deps/dpf)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED gtk+-3.0)
message(STATUS "GTK_LIBRARIES: ${GTK_LIBRARIES}")
message(STATUS "GTK_LINK_LIBRARIES: ${GTK_LINK_LIBRARIES}")
include_directories(${GTK_INCLUDE_DIRS})

include_directories (plugin)

dpf_add_plugin (${PROJECT_NAME}
    TARGETS vst2 vst3 clap jack
    FILES_DSP
        plugin/NativeMenuDemoPlugin.cpp
    FILES_UI
        plugin/NativeMenuDemoUI.cpp
)

target_link_libraries (${PROJECT_NAME}-ui PRIVATE ${GTK_LIBRARIES})

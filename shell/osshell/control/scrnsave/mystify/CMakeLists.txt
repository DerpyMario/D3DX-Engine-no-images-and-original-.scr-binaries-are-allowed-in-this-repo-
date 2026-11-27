cmake_minimum_required(VERSION 3.15)
project(Mystify_scr CXX RC)

set(CMAKE_CXX_STANDARD 17)

add_executable(Mystify.scr WIN32
    ../src/settings.cpp
    ../src/noise.cpp
    ../src/engine.cpp
    ../src/screensaver.cpp
    ../Mystify.scr.rc
)
target_include_directories(Mystify.scr PRIVATE ../src)
target_compile_definitions(Mystify.scr PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(Mystify.scr PRIVATE d3d9 comctl32 user32 gdi32 advapi32 uuid)

# The .mui satellite is a resource-only Win32 DLL (no entry point). MSVC only.
if(MSVC)
    enable_language(RC)
    add_library(Mystify.scr.mui MODULE
        ../Mystify.scr.mui.rc
    )
    set_target_properties(Mystify.scr.mui PROPERTIES
        LINK_FLAGS "/NOENTRY /BASE:0x10000000"
        OUTPUT_NAME "Mystify.scr.mui"
    )
endif()

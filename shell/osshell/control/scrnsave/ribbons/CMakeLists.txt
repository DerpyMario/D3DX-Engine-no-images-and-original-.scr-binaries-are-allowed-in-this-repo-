cmake_minimum_required(VERSION 3.15)
project(Ribbons_scr CXX RC)

set(CMAKE_CXX_STANDARD 17)

add_executable(Ribbons.scr WIN32
    ../src/settings.cpp
    ../src/noise.cpp
    ../src/engine.cpp
    ../src/screensaver.cpp
    ../Ribbons.scr.rc
)
target_include_directories(Ribbons.scr PRIVATE ../src)
target_compile_definitions(Ribbons.scr PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(Ribbons.scr PRIVATE d3d9 comctl32 user32 gdi32 advapi32 uuid)

# The .mui satellite is a resource-only Win32 DLL (no entry point). MSVC only.
if(MSVC)
    enable_language(RC)
    add_library(Ribbons.scr.mui MODULE
        ../Ribbons.scr.mui.rc
    )
    set_target_properties(Ribbons.scr.mui PROPERTIES
        LINK_FLAGS "/NOENTRY /BASE:0x10000000"
        OUTPUT_NAME "Ribbons.scr.mui"
    )
endif()

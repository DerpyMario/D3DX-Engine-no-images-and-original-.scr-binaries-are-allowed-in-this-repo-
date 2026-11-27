#------------------------------------------------------------------------------
# Original screen savers (not reconstructions)
#
#   Filaments.scr   glowing filaments advected through a curl-noise flow field,
#                   leaving fading trails.  Direct3D 9, fixed-function, no D3DX.
#
# This is a standalone CMake project; it does not depend on the reconstructed
# sources in the rest of the repository.
#
#   cmake -S original -B build-original -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-original
#
# Visual Studio works too (no toolchain file needed).  The result is
# Filaments.scr; copy it to %WINDIR%\System32 (or right-click > Install) to use
# it as a screen saver.
#------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)

project(OriginalScreenSavers LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "These are Windows programs: use a Windows compiler, or cross-compile with "
                        "-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake.")
endif()

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/W3>)
else()
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wall>)
    add_link_options(-static -mwindows)
endif()

add_executable(Filaments WIN32
    filaments/filaments.cpp
    filaments/flowfield.cpp
    filaments/filaments.rc)
set_target_properties(Filaments PROPERTIES OUTPUT_NAME Filaments SUFFIX ".scr")
target_link_libraries(Filaments PRIVATE d3d9 comctl32 gdi32 user32 advapi32)
install(TARGETS Filaments RUNTIME DESTINATION .)

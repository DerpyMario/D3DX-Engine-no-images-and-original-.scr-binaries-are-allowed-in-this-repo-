#------------------------------------------------------------------------------
# Toolchain file: cross-build the screen savers with MinGW-w64 for x86
# (32-bit), as the Windows XP Direct3D 8 savers are.
#
#   cmake -S winxp -B build-xp -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-i686.cmake
#
# Not needed for a native MinGW-w64 (MSYS2 mingw32) or Visual Studio build.
#------------------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR X86)

set(MINGW_PREFIX i686-w64-mingw32 CACHE STRING "Prefix of the MinGW-w64 tools")

set(CMAKE_C_COMPILER   ${MINGW_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${MINGW_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${MINGW_PREFIX}-windres)
set(CMAKE_DLLTOOL      ${MINGW_PREFIX}-dlltool)

set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

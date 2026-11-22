#------------------------------------------------------------------------------
# Windows XP / Windows Server 2003 screen savers - reconstructed sources
#
#   scrnsave.scr    (Blank)                 GDI
#   ssstars.scr     Starfield               GDI, scrnsavw.lib
#   ssmyst.scr      Mystify                 GDI, scrnsavw.lib
#   ssbezier.scr    Beziers                 GDI, scrnsavw.lib
#   ssmarque.scr    Marquee                 GDI, scrnsavw.lib
#   logon.scr       Windows XP / 2003 logo  GDI, scrnsavw.lib
#   ssmypics.scr    My Pictures Slideshow   GDI+, scrnsavw.lib
#   ssflwbox.scr    3D FlowerBox            Direct3D 8, d3dsaver.lib
#   sstext3d.scr    3D Text                 Direct3D 8, d3dsaver.lib
#   sspipes.scr     3D Pipes                Direct3D 8, d3dsaver.lib
#   ss3dfo.scr      3D Flying Objects       Direct3D 8, d3dsaver.lib
#
# The sources keep the layout of the original source tree,
# shell/osshell/control/scrnsave.  The Direct3D 8 savers are 32-bit programs
# even in the x64 editions of Windows (which install them as wss*.scr), so
# build them with a 32-bit compiler.  See BUILDING.md:
#
#   cmake -S winxp -B build-xp -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-i686.cmake
#   cmake --build build-xp
#
# The artwork of the savers is not part of this repository; extract it from
# your own copies with tools/extract_assets.py (into ./assets/winxp by
# default), or configure with -DSCRNSAVE_NO_ASSETS=ON.
#------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)

project(WinXPScreenSavers LANGUAGES C CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "These are Windows programs: use a Windows compiler, or cross-compile with "
                        "-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-i686.cmake (or -x86_64.cmake).")
endif()

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(D3D8_DEFAULT ON)
else()
    set(D3D8_DEFAULT OFF)
endif()

set(SCRNSAVE_ASSET_DIR "${REPO_ROOT}/assets/winxp" CACHE PATH
    "Directory written by tools/extract_assets.py for the Windows XP savers")
option(SCRNSAVE_NO_ASSETS "Build without the artwork of the original screen savers" OFF)
option(SCRNSAVE_D3D8 "Build the Direct3D 8 screen savers (32-bit programs in the originals)" ${D3D8_DEFAULT})

if(NOT SCRNSAVE_NO_ASSETS AND NOT EXISTS "${SCRNSAVE_ASSET_DIR}")
    message(FATAL_ERROR "No assets in ${SCRNSAVE_ASSET_DIR}.  Extract them from your own copies of the "
                        "screen savers (python tools/extract_assets.py <folder with the .scr files>), "
                        "point SCRNSAVE_ASSET_DIR at them, or configure with -DSCRNSAVE_NO_ASSETS=ON.")
endif()

if(SCRNSAVE_D3D8 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(WARNING "The Direct3D 8 savers are 32-bit programs in all the editions of Windows; "
                    "a 64-bit build of them needs a 64-bit d3d8.dll.")
endif()

set(SCRNSAVE ${CMAKE_CURRENT_SOURCE_DIR}/shell/osshell/control/scrnsave)

#------------------------------------------------------------------------------
# Compiler settings
#------------------------------------------------------------------------------
add_compile_definitions(UNICODE _UNICODE _WIN32_WINNT=0x0501 WINVER=0x0501)

# d3d8rgb.lib (the software rasterizer the Direct3D 8 savers register) is
# internal to the Windows source tree: build without it.
add_compile_definitions(NO_D3D8RGB)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/W3>)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

    if(SCRNSAVE_D3D8)
        # d3d8.h is not part of the current Windows SDK: it comes with the
        # DirectX 8.x SDKs (and with MinGW-w64).
        set(DX8SDK_DIR "$ENV{DX8SDK_DIR}" CACHE PATH "SDK with d3d8.h and d3d8.lib (Include, Lib)")
        if(NOT EXISTS "${DX8SDK_DIR}/Include/d3d8.h")
            message(FATAL_ERROR "d3d8.h not found: set DX8SDK_DIR to a DirectX 8 SDK, or configure "
                                "with -DSCRNSAVE_D3D8=OFF.")
        endif()
        include_directories(AFTER SYSTEM "${DX8SDK_DIR}/Include")
        link_directories("${DX8SDK_DIR}/Lib")
    endif()
else()
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:-Wall>)
    # call the C run-time functions, as the originals do (see ../CMakeLists.txt)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:-fno-builtin>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-overloaded-virtual>)
    # no libgcc / libstdc++ DLLs next to the screen savers
    add_link_options(-static)
endif()

if(SCRNSAVE_NO_ASSETS)
    add_compile_definitions(SCRNSAVE_NO_ASSETS)
endif()

#------------------------------------------------------------------------------
# Libraries
#------------------------------------------------------------------------------

# scrnsavw.lib - the screen saver library of the SDK (Unicode)
add_library(scrnsavw STATIC ${SCRNSAVE}/common/scrnsavw/scrnsave.c)
target_link_libraries(scrnsavw PUBLIC comctl32 user32 gdi32 advapi32)

# shlwapip.lib - IsOS by ordinal (not exported by name before Vista)
set(SHLWAPIP_DEF ${REPO_ROOT}/external/shlwapip/shlwapip.def)
if(MSVC)
    set(SHLWAPIP_LIB ${CMAKE_CURRENT_BINARY_DIR}/shlwapip.lib)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SHLWAPIP_MACHINE x64)
    else()
        set(SHLWAPIP_MACHINE x86)
    endif()
    add_custom_command(OUTPUT ${SHLWAPIP_LIB}
        COMMAND ${CMAKE_AR} /nologo /def:${SHLWAPIP_DEF} /machine:${SHLWAPIP_MACHINE} /out:${SHLWAPIP_LIB}
        DEPENDS ${SHLWAPIP_DEF} VERBATIM)
else()
    if(NOT CMAKE_DLLTOOL)
        find_program(CMAKE_DLLTOOL NAMES dlltool)
    endif()
    set(SHLWAPIP_LIB ${CMAKE_CURRENT_BINARY_DIR}/libshlwapip.a)
    add_custom_command(OUTPUT ${SHLWAPIP_LIB}
        COMMAND ${CMAKE_DLLTOOL} -d ${SHLWAPIP_DEF} -l ${SHLWAPIP_LIB}
        DEPENDS ${SHLWAPIP_DEF} VERBATIM)
endif()
add_custom_target(shlwapip_implib DEPENDS ${SHLWAPIP_LIB})

if(SCRNSAVE_D3D8)
    # d3dx8.lib - the part of D3DX 8 the savers call (external/d3dx8)
    add_library(d3dx8 STATIC
        ${REPO_ROOT}/external/d3dx8/src/d3dx8math.cpp
        ${REPO_ROOT}/external/d3dx8/src/d3dx8mesh.cpp
        ${REPO_ROOT}/external/d3dx8/src/d3dx8tex.cpp
        ${REPO_ROOT}/external/d3dx8/src/d3dx8text.cpp)
    target_include_directories(d3dx8 PUBLIC ${REPO_ROOT}/external/d3dx8/include)
    target_link_libraries(d3dx8 PUBLIC d3d8 glu32 opengl32 ole32 oleaut32 uuid gdi32 user32)

    # d3dsaver.lib - the Direct3D 8 screen saver framework
    add_library(d3dsaver STATIC
        ${SCRNSAVE}/d3dsaver/d3dsaver.cpp
        ${SCRNSAVE}/d3dsaver/dxutil.cpp)
    target_include_directories(d3dsaver PUBLIC ${SCRNSAVE}/d3dsaver)
    target_link_libraries(d3dsaver PUBLIC d3dx8 d3d8 comctl32 winmm gdi32 user32 advapi32)
endif()

#------------------------------------------------------------------------------
# The screen savers
#
#   add_screensaver(<target> OUTPUT_NAME <name> ASSETS <asset subdirectory>
#                   SOURCES <files> LIBRARIES <libraries>)
#------------------------------------------------------------------------------
function(add_screensaver target)
    cmake_parse_arguments(SS "" "OUTPUT_NAME;ASSETS" "SOURCES;LIBRARIES" ${ARGN})
    add_executable(${target} WIN32 ${SS_SOURCES})
    set_target_properties(${target} PROPERTIES OUTPUT_NAME ${SS_OUTPUT_NAME} SUFFIX ".scr")
    if(NOT SCRNSAVE_NO_ASSETS)
        # the resource scripts refer to "res/<file>"
        target_include_directories(${target} PRIVATE ${SCRNSAVE_ASSET_DIR}/${SS_ASSETS})
    endif()
    target_link_libraries(${target} PRIVATE ${SS_LIBRARIES})
    install(TARGETS ${target} RUNTIME DESTINATION .)
endfunction()

# The GDI savers

add_screensaver(scrnsave OUTPUT_NAME scrnsave ASSETS blank
    SOURCES ${SCRNSAVE}/blank/blank.c ${SCRNSAVE}/blank/blank.rc
    LIBRARIES gdi32 user32)

add_screensaver(ssstars OUTPUT_NAME ssstars ASSETS stars
    SOURCES ${SCRNSAVE}/stars/stars.c ${SCRNSAVE}/common/comstr.c ${SCRNSAVE}/stars/stars.rc
    LIBRARIES scrnsavw)

add_screensaver(ssmyst OUTPUT_NAME ssmyst ASSETS mystify
    SOURCES ${SCRNSAVE}/mystify/mystify.c ${SCRNSAVE}/common/comstr.c ${SCRNSAVE}/mystify/mystify.rc
    LIBRARIES scrnsavw)

add_screensaver(ssbezier OUTPUT_NAME ssbezier ASSETS bezier
    SOURCES ${SCRNSAVE}/bezier/bezier.c ${SCRNSAVE}/common/comstr.c ${SCRNSAVE}/bezier/bezier.rc
    LIBRARIES scrnsavw)

add_screensaver(ssmarque OUTPUT_NAME ssmarque ASSETS marquee
    SOURCES ${SCRNSAVE}/marquee/marquee.c ${SCRNSAVE}/common/comstr.c ${SCRNSAVE}/marquee/marquee.rc
    LIBRARIES scrnsavw comdlg32)

add_screensaver(logon OUTPUT_NAME logon ASSETS logon
    SOURCES ${SCRNSAVE}/logon/logon.c ${SCRNSAVE}/logon/generic.rc
    LIBRARIES scrnsavw ${SHLWAPIP_LIB} shlwapi ntdll)
add_dependencies(logon shlwapip_implib)

add_screensaver(ssmypics OUTPUT_NAME ssmypics ASSETS mypics
    SOURCES
        ${SCRNSAVE}/mypics/ssmypics.cpp
        ${SCRNSAVE}/mypics/mypics.cpp
        ${SCRNSAVE}/mypics/slideshow.cpp
        ${SCRNSAVE}/mypics/settings.cpp
        ${SCRNSAVE}/mypics/transitions.cpp
        ${SCRNSAVE}/mypics/slideimage.cpp
        ${SCRNSAVE}/mypics/findimages.cpp
        ${SCRNSAVE}/mypics/gdiphelp.cpp
        ${SCRNSAVE}/mypics/util.cpp
        ${SCRNSAVE}/mypics/ssmypics.rc
    LIBRARIES scrnsavw gdiplus msimg32 shlwapi shell32 ole32 uuid comctl32)
if(NOT MSVC)
    # operator new returns NULL when there is no memory, as in the original,
    # and the code checks for it: GCC must not assume otherwise
    target_compile_options(ssmypics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fcheck-new>)
endif()

# The Direct3D 8 savers

if(SCRNSAVE_D3D8)
    add_screensaver(ssflwbox OUTPUT_NAME ssflwbox ASSETS flowerbox
        SOURCES ${SCRNSAVE}/flowerbox/flowerbox.cpp ${SCRNSAVE}/flowerbox/flowerbox.rc
        LIBRARIES d3dsaver)

    add_screensaver(sstext3d OUTPUT_NAME sstext3d ASSETS text3d
        SOURCES ${SCRNSAVE}/text3d/text3d.cpp ${SCRNSAVE}/text3d/text3d.rc
        LIBRARIES d3dsaver comdlg32)

    add_screensaver(sspipes OUTPUT_NAME sspipes ASSETS pipes
        SOURCES
            ${SCRNSAVE}/pipes/stdafx.cpp
            ${SCRNSAVE}/pipes/eval.cpp
            ${SCRNSAVE}/pipes/fpipe.cpp
            ${SCRNSAVE}/pipes/fstate.cpp
            ${SCRNSAVE}/pipes/material.cpp
            ${SCRNSAVE}/pipes/node.cpp
            ${SCRNSAVE}/pipes/npipe.cpp
            ${SCRNSAVE}/pipes/nstate.cpp
            ${SCRNSAVE}/pipes/objects.cpp
            ${SCRNSAVE}/pipes/pipe.cpp
            ${SCRNSAVE}/pipes/pipes.cpp
            ${SCRNSAVE}/pipes/state.cpp
            ${SCRNSAVE}/pipes/view.cpp
            ${SCRNSAVE}/pipes/xc.cpp
            ${SCRNSAVE}/pipes/pipes.rc
        LIBRARIES d3dsaver comdlg32)

    add_screensaver(ss3dfo OUTPUT_NAME ss3dfo ASSETS 3dfo
        SOURCES
            ${SCRNSAVE}/3dfo/ss3dfo.cpp
            ${SCRNSAVE}/3dfo/splash.cpp
            ${SCRNSAVE}/3dfo/explode.cpp
            ${SCRNSAVE}/3dfo/twist.cpp
            ${SCRNSAVE}/3dfo/ribbon.cpp
            ${SCRNSAVE}/3dfo/flag.cpp
            ${SCRNSAVE}/3dfo/winlogo.cpp
            ${SCRNSAVE}/3dfo/mesh.cpp
            ${SCRNSAVE}/3dfo/texture.cpp
            ${SCRNSAVE}/3dfo/ss3dfo.rc
        LIBRARIES d3dsaver comdlg32)
endif()

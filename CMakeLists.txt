#------------------------------------------------------------------------------
# Windows Vista screen savers - reconstructed sources
#
#   Aurora.scr      Aurora
#   Bubbles.scr     Bubbles
#   ssText3d.scr    3D Text
#   ssBranded.scr   Windows Energy
#   logon.scr       Windows Logo
#
# The sources keep the layout of the original source tree,
# shell/osshell/control/scrnsave.  See BUILDING.md for the details:
#
#   cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
#   cmake --build build
#
# The artwork of the savers is not part of this repository; extract it from
# your own copies with tools/extract_assets.py (into ./assets by default), or
# configure with -DSCRNSAVE_NO_ASSETS=ON.
#------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)

project(VistaScreenSavers LANGUAGES C CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "These are Windows programs: use a Windows compiler, or cross-compile with "
                        "-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(WARNING "The originals are x64 programs; the layout checks (static_assert) only apply to "
                    "x64 builds, and the import library of winbrand.dll is made for x64.")
endif()

set(SCRNSAVE_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets" CACHE PATH
    "Directory written by tools/extract_assets.py")
option(SCRNSAVE_NO_ASSETS "Build without the artwork of the original screen savers" OFF)
option(SCRNSAVE_BUILD_TESTS "Build the differential tests of tools/verify (MinGW-w64 only)" OFF)

if(NOT SCRNSAVE_NO_ASSETS AND NOT EXISTS "${SCRNSAVE_ASSET_DIR}")
    message(FATAL_ERROR "No assets in ${SCRNSAVE_ASSET_DIR}.  Extract them from your own copies of the "
                        "screen savers (python tools/extract_assets.py C:\\Windows\\System32), point "
                        "SCRNSAVE_ASSET_DIR at them, or configure with -DSCRNSAVE_NO_ASSETS=ON.")
endif()

set(SCRNSAVE ${CMAKE_CURRENT_SOURCE_DIR}/shell/osshell/control/scrnsave)
set(VERIFY ${CMAKE_CURRENT_SOURCE_DIR}/tools/verify)

#------------------------------------------------------------------------------
# Compiler settings
#------------------------------------------------------------------------------
add_compile_definitions(UNICODE _UNICODE _WIN32_WINNT=0x0600 WINVER=0x0600)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/W3>)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

    # d3dx9 is not part of the Windows SDK: it comes with the DirectX SDK
    # (June 2010).  Its headers go after the Windows SDK's.
    set(DXSDK_DIR "$ENV{DXSDK_DIR}" CACHE PATH "DirectX SDK (June 2010), for d3dx9")
    if(NOT EXISTS "${DXSDK_DIR}/Include/d3dx9.h")
        message(FATAL_ERROR "d3dx9.h not found: install the DirectX SDK (June 2010) or set DXSDK_DIR.")
    endif()
    include_directories(AFTER SYSTEM "${DXSDK_DIR}/Include")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        link_directories("${DXSDK_DIR}/Lib/x64")
    else()
        link_directories("${DXSDK_DIR}/Lib/x86")
    endif()
else()
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:-Wall>)
    # Call the C run-time functions of msvcrt.dll, as the originals do: GCC
    # would otherwise fold sinf()/cosf() pairs into its own sincosf() and
    # expand other functions inline, with (slightly) different results.
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:-fno-builtin>)
    # the class hierarchies hide overloaded virtual functions, as the
    # originals do
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-overloaded-virtual>)
    # no libgcc / libstdc++ DLLs next to the screen savers
    add_link_options(-static)
endif()

if(SCRNSAVE_NO_ASSETS)
    add_compile_definitions(SCRNSAVE_NO_ASSETS)
endif()

#------------------------------------------------------------------------------
# Libraries of the source tree
#------------------------------------------------------------------------------

# D3DSaver9.lib - the Direct3D 9 screen saver framework
add_library(D3DSaver9 STATIC
    ${SCRNSAVE}/d3dsaver9/d3dsaver9.cpp
    ${SCRNSAVE}/d3dsaver9/dxutil9.cpp)
target_include_directories(D3DSaver9 PUBLIC ${SCRNSAVE}/d3dsaver9)
target_link_libraries(D3DSaver9 PUBLIC d3dx9 d3d9 comctl32 winmm ole32 gdi32 user32 advapi32)

# Helpers.lib - textures, Targa images, noise, matrices, colours, particles
add_library(Helpers STATIC
    ${SCRNSAVE}/helper/color.cpp
    ${SCRNSAVE}/helper/image.cpp
    ${SCRNSAVE}/helper/matrix44.cpp
    ${SCRNSAVE}/helper/noise.cpp
    ${SCRNSAVE}/helper/particle.cpp
    ${SCRNSAVE}/helper/targa.cpp
    ${SCRNSAVE}/helper/texture.cpp)
target_include_directories(Helpers PUBLIC ${SCRNSAVE}/helper)
target_link_libraries(Helpers PUBLIC D3DSaver9 windowscodecs ole32 uuid)

# Shape3D.lib - 3D text meshes from TrueType outlines
add_library(Shape3D STATIC
    ${SCRNSAVE}/shape3d/fontoutl.cpp
    ${SCRNSAVE}/shape3d/fontextr.cpp)
target_include_directories(Shape3D PUBLIC ${SCRNSAVE}/shape3d)
target_link_libraries(Shape3D PUBLIC d3dx9 d3d9 usp10 glu32 opengl32 gdi32)

# scrnsavw.lib - the classic (GDI) screen saver library
add_library(scrnsavw STATIC ${SCRNSAVE}/common/scrnsavw/scrnsave.c)
target_link_libraries(scrnsavw PUBLIC comctl32 user32 gdi32 advapi32)

# winbrand.lib - import library of the Windows branding DLL
set(WINBRAND_DEF ${CMAKE_CURRENT_SOURCE_DIR}/external/winbrand/winbrand.def)
if(MSVC)
    set(WINBRAND_LIB ${CMAKE_CURRENT_BINARY_DIR}/winbrand.lib)
    set(WINBRAND_DELAY_LIB ${WINBRAND_LIB})
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WINBRAND_MACHINE x64)
    else()
        set(WINBRAND_MACHINE x86)
    endif()
    add_custom_command(OUTPUT ${WINBRAND_LIB}
        COMMAND ${CMAKE_AR} /nologo /def:${WINBRAND_DEF} /machine:${WINBRAND_MACHINE} /out:${WINBRAND_LIB}
        DEPENDS ${WINBRAND_DEF} VERBATIM)
else()
    if(NOT CMAKE_DLLTOOL)
        find_program(CMAKE_DLLTOOL NAMES dlltool)
    endif()
    set(WINBRAND_LIB ${CMAKE_CURRENT_BINARY_DIR}/libwinbrand.a)
    set(WINBRAND_DELAY_LIB ${CMAKE_CURRENT_BINARY_DIR}/libwinbrand_delay.a)
    add_custom_command(OUTPUT ${WINBRAND_LIB} ${WINBRAND_DELAY_LIB}
        COMMAND ${CMAKE_DLLTOOL} -d ${WINBRAND_DEF} -l ${WINBRAND_LIB}
        COMMAND ${CMAKE_DLLTOOL} -d ${WINBRAND_DEF} -y ${WINBRAND_DELAY_LIB}
        DEPENDS ${WINBRAND_DEF} VERBATIM)
endif()
add_custom_target(winbrand_implib DEPENDS ${WINBRAND_LIB} ${WINBRAND_DELAY_LIB})

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

add_screensaver(Aurora OUTPUT_NAME Aurora ASSETS aurora
    SOURCES
        ${SCRNSAVE}/aurora/arc.cpp
        ${SCRNSAVE}/aurora/aurora.cpp
        ${SCRNSAVE}/aurora/auroraconfig.cpp
        ${SCRNSAVE}/aurora/camera.cpp
        ${SCRNSAVE}/aurora/aurora.rc
    LIBRARIES D3DSaver9 Helpers)

add_screensaver(Bubbles OUTPUT_NAME Bubbles ASSETS bubbles
    SOURCES
        ${SCRNSAVE}/bubbles/bubbles.cpp
        ${SCRNSAVE}/bubbles/bubblesconfig.cpp
        ${SCRNSAVE}/bubbles/bubbles.rc
    LIBRARIES D3DSaver9 Helpers)

add_screensaver(ssText3d OUTPUT_NAME ssText3d ASSETS text3d
    SOURCES
        ${SCRNSAVE}/text3d/text3d.cpp
        ${SCRNSAVE}/text3d/text3d.rc
    LIBRARIES D3DSaver9 Helpers Shape3D ${WINBRAND_LIB} comdlg32)
target_include_directories(ssText3d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/winbrand)
add_dependencies(ssText3d winbrand_implib)

file(GLOB BRANDED_SOURCES ${SCRNSAVE}/branded/src/*.cpp)
add_screensaver(ssBranded OUTPUT_NAME ssBranded ASSETS branded
    SOURCES ${BRANDED_SOURCES} ${SCRNSAVE}/branded/src/ssbranded.rc
    LIBRARIES D3DSaver9 Helpers)
target_include_directories(ssBranded PRIVATE ${SCRNSAVE}/branded/src)

# logon.scr loads winbrand.dll only when it is needed (delay load)
add_screensaver(logon OUTPUT_NAME logon ASSETS logon
    SOURCES
        ${SCRNSAVE}/logon/logon.cpp
        ${SCRNSAVE}/logon/generic.rc
    LIBRARIES scrnsavw ${WINBRAND_DELAY_LIB})
target_include_directories(logon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/winbrand)
add_dependencies(logon winbrand_implib)
if(MSVC)
    target_link_libraries(logon PRIVATE delayimp)
    target_link_options(logon PRIVATE /DELAYLOAD:winbrand.dll)
endif()

#------------------------------------------------------------------------------
# Differential tests (tools/verify)
#
# Each test maps YOUR original screen saver into its process and compares
# functions of it with the rebuilt ones:  test_<name> <path to the original>.
# They rely on GNU ld (--wrap), so they are built with MinGW-w64 only.
#------------------------------------------------------------------------------
if(SCRNSAVE_BUILD_TESTS)
    if(MSVC)
        message(FATAL_ERROR "The tests of tools/verify are built with MinGW-w64 (they need GNU ld's --wrap).")
    endif()

    # both sides call the same msvcrt.dll math functions
    set(MSVCRT_MATH_WRAP
        -Wl,--wrap=sinf,--wrap=cosf,--wrap=tanf,--wrap=acosf,--wrap=asinf,--wrap=atanf,--wrap=sqrtf
        -Wl,--wrap=expf,--wrap=logf,--wrap=floorf,--wrap=ceilf,--wrap=atan2f,--wrap=powf,--wrap=fmodf)

    function(add_verify_test name)
        cmake_parse_arguments(T "" "" "SOURCES;LIBRARIES;INCLUDES;LINK_OPTIONS" ${ARGN})
        add_executable(test_${name} ${VERIFY}/test_${name}.cpp ${VERIFY}/msvcrtmath.cpp ${T_SOURCES})
        target_include_directories(test_${name} PRIVATE ${VERIFY} ${T_INCLUDES})
        # the tests embed the resources of their saver, without the artwork
        target_compile_definitions(test_${name} PRIVATE SCRNSAVE_NO_ASSETS)
        target_link_options(test_${name} PRIVATE -Wl,--allow-multiple-definition ${MSVCRT_MATH_WRAP}
                            ${T_LINK_OPTIONS})
        target_link_libraries(test_${name} PRIVATE ${T_LIBRARIES} d3dx9 d3d9 comctl32 comdlg32 winmm ole32
                              oleaut32 gdi32 advapi32 user32 windowscodecs uuid)
    endfunction()

    add_verify_test(aurora
        SOURCES
            ${SCRNSAVE}/aurora/arc.cpp
            ${SCRNSAVE}/aurora/aurora.cpp
            ${SCRNSAVE}/aurora/auroraconfig.cpp
            ${SCRNSAVE}/aurora/camera.cpp
            ${SCRNSAVE}/aurora/aurora.rc
        INCLUDES ${SCRNSAVE}/aurora
        LIBRARIES D3DSaver9 Helpers)

    add_verify_test(bubbles
        SOURCES
            ${SCRNSAVE}/bubbles/bubbles.cpp
            ${SCRNSAVE}/bubbles/bubblesconfig.cpp
            ${SCRNSAVE}/bubbles/bubbles.rc
        INCLUDES ${SCRNSAVE}/bubbles
        LIBRARIES D3DSaver9 Helpers)

    add_verify_test(text3d
        SOURCES
            ${SCRNSAVE}/text3d/text3d.cpp
            ${SCRNSAVE}/text3d/text3d.rc
        INCLUDES ${SCRNSAVE}/text3d ${CMAKE_CURRENT_SOURCE_DIR}/external/winbrand
        LIBRARIES D3DSaver9 Helpers Shape3D ${WINBRAND_DELAY_LIB})
    add_dependencies(test_text3d winbrand_implib)

    add_verify_test(logon
        SOURCES
            ${SCRNSAVE}/logon/logon.cpp
            ${SCRNSAVE}/logon/generic.rc
        INCLUDES ${SCRNSAVE}/logon ${CMAKE_CURRENT_SOURCE_DIR}/external/winbrand
        LIBRARIES scrnsavw ${WINBRAND_DELAY_LIB})
    add_dependencies(test_logon winbrand_implib)

    add_verify_test(branded
        SOURCES ${BRANDED_SOURCES} ${SCRNSAVE}/branded/src/ssbranded.rc
        INCLUDES ${SCRNSAVE}/branded/src
        LIBRARIES D3DSaver9 Helpers
        LINK_OPTIONS
            -Wl,--wrap=D3DXMatrixMultiply,--wrap=D3DXMatrixRotationQuaternion,--wrap=D3DXMatrixTranslation
            -Wl,--wrap=D3DXQuaternionInverse,--wrap=D3DXQuaternionSlerp,--wrap=D3DXMatrixDecompose
            -Wl,--wrap=D3DXVec3Normalize,--wrap=D3DXMatrixRotationYawPitchRoll,--wrap=D3DXMatrixLookAtLH)

    # Windows Energy with the effect and mesh loaders of the original's D3DX,
    # for systems whose d3dx9 cannot compile its effects (Wine); put your
    # original next to it as ssBranded.orig.scr.
    add_executable(ssBranded_origd3dx WIN32 ${BRANDED_SOURCES} ${SCRNSAVE}/branded/src/ssbranded.rc
                   ${VERIFY}/branded_origd3dx.cpp)
    set_target_properties(ssBranded_origd3dx PROPERTIES OUTPUT_NAME ssBranded SUFFIX ".scr"
                          RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/origd3dx)
    target_include_directories(ssBranded_origd3dx PRIVATE ${SCRNSAVE}/branded/src ${VERIFY})
    if(NOT SCRNSAVE_NO_ASSETS)
        target_include_directories(ssBranded_origd3dx PRIVATE ${SCRNSAVE_ASSET_DIR}/branded)
    endif()
    target_link_options(ssBranded_origd3dx PRIVATE
        -Wl,--wrap=D3DXCreateEffectFromResourceW,--wrap=D3DXLoadMeshFromXResource
        -Wl,--wrap=D3DXGetShaderOutputSemantics)
    target_link_libraries(ssBranded_origd3dx PRIVATE D3DSaver9 Helpers)
endif()

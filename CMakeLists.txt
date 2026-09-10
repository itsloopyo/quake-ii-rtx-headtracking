cmake_minimum_required(VERSION 3.20)
project(QuakeIIRTXHeadTracking VERSION 0.0.0 LANGUAGES C CXX)

# q2rtx.exe is x64 and Ultimate ASI Loader will not load a 32-bit plugin into
# it. It fails silently - no tracking, and no QuakeIIRTXHeadTracking.log to say
# why - so the wrong pointer size is refused here rather than at the player.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR
        "This mod must be built x64: q2rtx.exe is 64-bit and its loader ignores a "
        "32-bit .asi. Configure with -A x64.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Shared CameraUnlock C++ core
add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

set(MINHOOK_SOURCES
    extern/minhook/src/buffer.c
    extern/minhook/src/hook.c
    extern/minhook/src/trampoline.c
    extern/minhook/src/hde/hde32.c
    extern/minhook/src/hde/hde64.c
)

# Pure logic, no engine memory and no Windows: shared with the test target so
# the axis boundary, the reticle placement and the build registry that decides
# whether the mod engages at all are exercised off the game.
set(MOD_PURE_SOURCES
    src/quake_math.cpp
    src/hooks/reticle.cpp
    src/core/build_profiles.cpp
    src/core/steam_offsets.cpp
)

# Touches no engine memory either, but does touch the filesystem and the Win32
# profile API, so it is kept apart from MOD_PURE_SOURCES. Shared with the test
# target because HeadTracking.ini is the mod's one untrusted input.
set(MOD_TESTABLE_SOURCES
    src/core/config.cpp
)

set(MOD_SOURCES
    src/dllmain.cpp
    src/core/mod.cpp
    src/engine/fov_zoom.cpp
    src/engine/game_state.cpp
    src/engine/lean_trace.cpp
    src/engine/slot_swap.cpp
    src/window_centering.cpp
    src/hooks/render_hook.cpp
    src/hooks/crosshair_hook.cpp
    cameraunlock-core/cpp/src/hooks/hook_manager.cpp
)

add_library(QuakeIIRTXHeadTracking SHARED
    ${MINHOOK_SOURCES}
    ${MOD_SOURCES}
    ${MOD_TESTABLE_SOURCES}
    ${MOD_PURE_SOURCES}
)

set_target_properties(QuakeIIRTXHeadTracking PROPERTIES
    OUTPUT_NAME "QuakeIIRTXHeadTracking"
    SUFFIX ".asi"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

target_link_libraries(QuakeIIRTXHeadTracking PRIVATE cameraunlock)

target_include_directories(QuakeIIRTXHeadTracking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/minhook/include
)

if(WIN32)
    target_link_libraries(QuakeIIRTXHeadTracking PRIVATE ws2_32)
endif()

# Characterization tests for this mod's own logic. The core library brings its
# own suite; this one covers the tracker-to-Quake axis boundary and the reticle
# projection and placement, whose signs were established by measurement in game,
# plus the HeadTracking.ini boundary, which is the only untrusted input the mod
# has.
option(Q2RTXHT_BUILD_TESTS "Build the mod's own tests" ON)
if(Q2RTXHT_BUILD_TESTS)
    enable_testing()
    add_executable(quake_ii_rtx_headtracking_tests
        tests/test_main.cpp
        tests/quake_math_tests.cpp
        tests/reticle_tests.cpp
        tests/config_tests.cpp
        tests/build_profile_tests.cpp
        ${MOD_TESTABLE_SOURCES}
        ${MOD_PURE_SOURCES}
    )
    target_include_directories(quake_ii_rtx_headtracking_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    # The shipped .ini, Config::WriteDefault and launcher-manifest.json's
    # loader.seed are three copies of one document. The suite compares them byte
    # for byte, so it needs the two tracked ones.
    target_compile_definitions(quake_ii_rtx_headtracking_tests PRIVATE
        Q2RTXHT_SHIPPED_INI="${CMAKE_CURRENT_SOURCE_DIR}/config/HeadTracking.ini"
        Q2RTXHT_MANIFEST="${CMAKE_CURRENT_SOURCE_DIR}/launcher-manifest.json"
        Q2RTXHT_PROJECT_VERSION="${PROJECT_VERSION}"
        Q2RTXHT_INSTALL_CMD="${CMAKE_CURRENT_SOURCE_DIR}/scripts/install.cmd"
    )
    target_link_libraries(quake_ii_rtx_headtracking_tests PRIVATE cameraunlock)
    add_test(NAME quake_ii_rtx_headtracking_tests
             COMMAND quake_ii_rtx_headtracking_tests)
endif()

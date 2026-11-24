cmake_minimum_required(VERSION 3.21)

project(WarbandLib VERSION 0.1.0 DESCRIPTION "Plugin runtime and stable C ABI for Mount & Blade: Warband" LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WARBANDLIB_BUILD_EXAMPLES "Build the example plugins" ON)
option(WARBANDLIB_BUILD_TESTS "Build unit tests (only when not cross-compiling)" ON)
set(WARBANDLIB_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/internal" CACHE PATH "Private data (the internal/ submodule); the runtime builds without it")

set(WARBANDLIB_HAS_DATA FALSE)
if(EXISTS "${WARBANDLIB_DATA_DIR}/catalogue/mb_warband.ini")
    set(WARBANDLIB_HAS_DATA TRUE)
endif()

set(WARBANDLIB_TARGETS_WINDOWS FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WARBANDLIB_TARGETS_WINDOWS TRUE)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "mb_warband.exe is 32-bit: configure an x86 build (see CMakePresets.json).")
    endif()
endif()

if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(WARBANDLIB_PLUGIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/plugins)

add_library(warbandlib_options INTERFACE)
target_include_directories(warbandlib_options INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(warbandlib_options INTERFACE
    $<$<BOOL:${WARBANDLIB_TARGETS_WINDOWS}>:WIN32_LEAN_AND_MEAN>
    $<$<BOOL:${WARBANDLIB_TARGETS_WINDOWS}>:NOMINMAX>
    $<$<BOOL:${WARBANDLIB_TARGETS_WINDOWS}>:_CRT_SECURE_NO_WARNINGS>
)
if(MSVC)
    target_compile_options(warbandlib_options INTERFACE /W4 /utf-8)
else()
    target_compile_options(warbandlib_options INTERFACE -Wall -Wextra -Wpedantic)
endif()

# sdk: the public C ABI. Header-only; abi_lock.c fails the build if the x86 layout changes.
add_library(warbandlib_sdk INTERFACE)
add_library(WarbandLib::sdk ALIAS warbandlib_sdk)
target_include_directories(warbandlib_sdk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)

add_library(warbandlib_abi_lock OBJECT sdk/abi_lock.c)
target_link_libraries(warbandlib_abi_lock PRIVATE warbandlib_sdk)

# Platform-independent pieces, shared by the runtime and the unit tests.
add_library(warbandlib_core STATIC
    core/aob_scanner.cpp
    core/logging.cpp
    core/sha256.cpp
    runtime/host/semver.cpp
    runtime/host/plugin_graph.cpp
    runtime/host/event_bus.cpp
    runtime/host/hook_chain.cpp
    runtime/host/interface_registry.cpp
)
target_link_libraries(warbandlib_core PUBLIC warbandlib_options warbandlib_sdk)

if(WARBANDLIB_TARGETS_WINDOWS)
    include(FetchContent)

    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.60
        GIT_SHALLOW TRUE
    )
    FetchContent_Declare(
        minhook
        GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
        GIT_TAG v1.3.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui minhook)

    # ImGui 1.60 ships no CMakeLists.txt; its DX9 binding (Win32 input included) lives under examples/.
    add_library(warbandlib_imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/examples/directx9_example/imgui_impl_dx9.cpp
    )
    target_include_directories(warbandlib_imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/examples/directx9_example
    )
    target_compile_definitions(warbandlib_imgui PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(warbandlib_imgui PRIVATE /W0)
    endif()

    add_library(warbandlib_core_win32 STATIC
        core/win32/memory.cpp
        core/win32/module.cpp
    )
    target_link_libraries(warbandlib_core_win32 PUBLIC warbandlib_core)

    add_library(warbandlib_runtime SHARED
        runtime/dllmain.cpp
        runtime/platform/host_api.cpp
        runtime/platform/hook_manager.cpp
        runtime/platform/memory_access.cpp
        runtime/platform/overlay.cpp
        runtime/platform/plugin_manager.cpp
        $<TARGET_OBJECTS:warbandlib_abi_lock>
    )
    target_link_libraries(warbandlib_runtime PRIVATE
        warbandlib_core
        warbandlib_core_win32
        warbandlib_imgui
        minhook
        d3d9
        user32
    )
    set_target_properties(warbandlib_runtime PROPERTIES PREFIX "" OUTPUT_NAME "warbandlib_runtime")
    if(WARBANDLIB_HAS_DATA)
        add_custom_command(TARGET warbandlib_runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${WARBANDLIB_DATA_DIR}/catalogue/mb_warband.ini
                $<TARGET_FILE_DIR:warbandlib_runtime>/mb_warband.ini
        )
    endif()

    add_executable(warbandlib_injector tools/injector/main.cpp)
    target_link_libraries(warbandlib_injector PRIVATE warbandlib_options)
    set_target_properties(warbandlib_injector PROPERTIES OUTPUT_NAME "injector")

    # A plugin links only the SDK headers (plus whatever it needs itself), never the runtime.
    function(warbandlib_add_plugin name)
        add_library(${name} SHARED ${ARGN})
        target_link_libraries(${name} PRIVATE warbandlib_sdk warbandlib_options)
        set_target_properties(${name} PROPERTIES
            PREFIX ""
            RUNTIME_OUTPUT_DIRECTORY ${WARBANDLIB_PLUGIN_OUTPUT_DIR}
            LIBRARY_OUTPUT_DIRECTORY ${WARBANDLIB_PLUGIN_OUTPUT_DIR}
        )
        foreach(config Debug Release RelWithDebInfo MinSizeRel)
            string(TOUPPER ${config} upper)
            set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${upper} ${WARBANDLIB_PLUGIN_OUTPUT_DIR})
        endforeach()
    endfunction()

    if(WARBANDLIB_BUILD_EXAMPLES)
        warbandlib_add_plugin(example_hello_c examples/hello_c/hello.c)
        warbandlib_add_plugin(example_hp_editor examples/hp_editor/hp_editor.cpp)
        warbandlib_add_plugin(example_overlay_quad examples/overlay_quad/overlay_quad.cpp)
        target_link_libraries(example_overlay_quad PRIVATE d3d9)
    endif()

    if(WARBANDLIB_HAS_DATA AND EXISTS "${WARBANDLIB_DATA_DIR}/CMakeLists.txt")
        add_subdirectory("${WARBANDLIB_DATA_DIR}" "${CMAKE_BINARY_DIR}/internal")
    endif()

    # Injects the already built runtime and plugins into the running game (Proton via protonhax); builds nothing.
    add_custom_target(inject
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tools/hot-reload.sh $<TARGET_FILE_DIR:warbandlib_runtime>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        USES_TERMINAL
        VERBATIM
    )
endif()

if(WARBANDLIB_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
            GIT_SHALLOW TRUE
        )
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
    enable_testing()
    include(GoogleTest)
    add_executable(warbandlib_tests
        tests/aob_scanner_tests.cpp
        tests/semver_tests.cpp
        tests/plugin_graph_tests.cpp
        tests/event_bus_tests.cpp
        tests/hook_chain_tests.cpp
        tests/interface_registry_tests.cpp
        tests/abi_tests.c
    )
    target_link_libraries(warbandlib_tests PRIVATE warbandlib_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests(warbandlib_tests)
endif()

message(STATUS "WarbandLib ${PROJECT_VERSION}: compiler ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}, "
               "targets Windows: ${WARBANDLIB_TARGETS_WINDOWS}, cross-compiling: ${CMAKE_CROSSCOMPILING}, private data: ${WARBANDLIB_HAS_DATA}")

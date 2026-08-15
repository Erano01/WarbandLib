cmake_minimum_required(VERSION 3.15)

project(WarbandLib VERSION 0.1.0 LANGUAGES C CXX)

option(WARBANDLIB_BUILD_RUNTIME "Build Windows runtime and hook layer" ON)
option(WARBANDLIB_BUILD_TOOLS "Build analysis and verification tools" ON)
option(WARBANDLIB_BUILD_EXAMPLES "Build example mods and integration samples" ON)
option(WARBANDLIB_BUILD_EXAMPLE_OVERLAY_QUAD "Build the overlay_quad example (safe, F9-toggleable EndScene draw demo)" ON)
option(WARBANDLIB_BUILD_TESTS "Build gtest-based unit tests (host-native only)" ON)
option(WARBANDLIB_ENABLE_C_ABI "Enable the portable C ABI layer for cross-toolchain development" ON)
option(WARBANDLIB_STRICT_WARNINGS "Enable strict warning levels" ON)

set(WARBANDLIB_TARGET_ARCH "x86" CACHE STRING "Target architecture for game-facing code")
set_property(CACHE WARBANDLIB_TARGET_ARCH PROPERTY STRINGS x86)

if(NOT WARBANDLIB_TARGET_ARCH STREQUAL "x86")
	message(FATAL_ERROR "WarbandLib currently supports only x86 game-facing targets.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(WARNING "You are configuring from a 64-bit toolchain or generator. Warband runtime code should be built with a 32-bit x86 toolchain.")
endif()

set(WARBANDLIB_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(WARBANDLIB_RE_DIR "${WARBANDLIB_ROOT_DIR}/re")
set(WARBANDLIB_SIGNATURES_DIR "${WARBANDLIB_ROOT_DIR}/signatures")
set(WARBANDLIB_CORE_DIR "${WARBANDLIB_ROOT_DIR}/core")
set(WARBANDLIB_RUNTIME_DIR "${WARBANDLIB_ROOT_DIR}/runtime")
set(WARBANDLIB_SDK_DIR "${WARBANDLIB_ROOT_DIR}/sdk")
set(WARBANDLIB_TOOLS_DIR "${WARBANDLIB_ROOT_DIR}/tools")
set(WARBANDLIB_EXAMPLES_DIR "${WARBANDLIB_ROOT_DIR}/examples")
set(WARBANDLIB_DOCS_DIR "${WARBANDLIB_ROOT_DIR}/docs")

# Targeting Windows means either cross-compiling from Linux with
# cmake/toolchain-mingw-i686.cmake, or configuring natively on Windows.
set(WARBANDLIB_TARGETING_WINDOWS FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(WARBANDLIB_TARGETING_WINDOWS TRUE)
endif()

add_library(warbandlib_project_options INTERFACE)
add_library(warbandlib_project_warnings INTERFACE)

target_compile_definitions(warbandlib_project_options
	INTERFACE
		WIN32_LEAN_AND_MEAN
		NOMINMAX
		_CRT_SECURE_NO_WARNINGS
		$<$<BOOL:${WARBANDLIB_ENABLE_C_ABI}>:WARBANDLIB_ENABLE_C_ABI=1>
)

target_include_directories(warbandlib_project_options
	INTERFACE
		"${WARBANDLIB_ROOT_DIR}"
		"${WARBANDLIB_CORE_DIR}"
		"${WARBANDLIB_RUNTIME_DIR}"
		"${WARBANDLIB_SDK_DIR}"
)

target_compile_features(warbandlib_project_options INTERFACE cxx_std_17)

if(MSVC)
	target_compile_definitions(warbandlib_project_options INTERFACE _WIN32_WINNT=0x0501)

	if(MSVC_VERSION LESS 1600)
		message(FATAL_ERROR "WarbandLib requires at least MSVC 2010 compatibility for Windows runtime work.")
	endif()

	if(WARBANDLIB_STRICT_WARNINGS)
		target_compile_options(warbandlib_project_warnings INTERFACE /W4)
	else()
		target_compile_options(warbandlib_project_warnings INTERFACE /W3)
	endif()

	target_compile_options(warbandlib_project_options
		INTERFACE
			/MP
			/EHsc
			/GR
			/Zi
	)
else()
	if(WARBANDLIB_TARGETING_WINDOWS)
		target_compile_definitions(warbandlib_project_options INTERFACE _WIN32_WINNT=0x0501)
	endif()

	if(WARBANDLIB_STRICT_WARNINGS)
		target_compile_options(warbandlib_project_warnings
			INTERFACE
				-Wall
				-Wextra
				-Wpedantic
				-Wconversion
				-Wshadow
		)
	else()
		target_compile_options(warbandlib_project_warnings INTERFACE -Wall -Wextra)
	endif()

	target_compile_options(warbandlib_project_options
		INTERFACE
			-fno-strict-aliasing
	)

	if(WARBANDLIB_TARGETING_WINDOWS)
		# Target machines won't have the MinGW runtime installed -- link it
		# in statically so warbandlib_runtime.dll / warbandlib_injector.exe
		# don't depend on libgcc_s_dw2-1.dll / libstdc++-6.dll being present.
		target_link_options(warbandlib_project_options
			INTERFACE
				-static-libgcc
				-static-libstdc++
				-static
		)
	endif()
endif()

# ---------------------------------------------------------------------------
# core: portable logic (aob scanner, logging, sha256). No Windows headers,
# builds and is unit-testable with the host (Linux-native) toolchain.
# ---------------------------------------------------------------------------
add_library(warbandlib_core STATIC
	"${WARBANDLIB_CORE_DIR}/aob_scanner.cpp"
	"${WARBANDLIB_CORE_DIR}/logging.cpp"
	"${WARBANDLIB_CORE_DIR}/sha256.cpp"
)
target_link_libraries(warbandlib_core
	PUBLIC
		warbandlib_project_options
		warbandlib_project_warnings
)

add_library(warbandlib_sdk INTERFACE)
target_link_libraries(warbandlib_sdk
	INTERFACE
		warbandlib_core
)

if(WARBANDLIB_ENABLE_C_ABI)
	add_library(warbandlib_cabi INTERFACE)
	target_link_libraries(warbandlib_cabi
		INTERFACE
			warbandlib_core
	)

	target_compile_definitions(warbandlib_cabi INTERFACE WARBANDLIB_C_API=1)
endif()

add_custom_target(warbandlib_layout ALL
	COMMENT "WarbandLib repository scaffold is configured"
)

# ---------------------------------------------------------------------------
# runtime: Windows-only. core_win32 (memory/module helpers), the vtable hook
# engine, and the injectable DLL itself. Only built when actually targeting
# Windows (native MSVC or cmake/toolchain-mingw-i686.cmake).
# ---------------------------------------------------------------------------
if(WARBANDLIB_BUILD_RUNTIME)
	if(WARBANDLIB_TARGETING_WINDOWS)
		add_library(warbandlib_core_win32 STATIC
			"${WARBANDLIB_CORE_DIR}/win32/memory.cpp"
			"${WARBANDLIB_CORE_DIR}/win32/module.cpp"
		)
		target_link_libraries(warbandlib_core_win32
			PUBLIC
				warbandlib_core
		)

		add_library(warbandlib_runtime SHARED
			"${WARBANDLIB_RUNTIME_DIR}/dllmain.cpp"
			"${WARBANDLIB_RUNTIME_DIR}/hooking/vtable_hook.cpp"
			"${WARBANDLIB_RUNTIME_DIR}/graphics/d3d9/endscene_hook.cpp"
			"${WARBANDLIB_RUNTIME_DIR}/graphics/d3d9/bootstrap.cpp"
		)
		target_link_libraries(warbandlib_runtime
			PRIVATE
				warbandlib_core
				warbandlib_core_win32
		)
		set_target_properties(warbandlib_runtime PROPERTIES
			PREFIX ""
			OUTPUT_NAME "warbandlib_runtime"
		)

		# The signature table is data the DLL reads at runtime (next to
		# itself), not something it links against.
		add_custom_command(TARGET warbandlib_runtime POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${WARBANDLIB_SIGNATURES_DIR}/mb_warband.ini"
				"$<TARGET_FILE_DIR:warbandlib_runtime>/mb_warband.ini"
		)

		add_dependencies(warbandlib_layout warbandlib_runtime)
	else()
		add_custom_target(warbandlib_runtime_stub
			COMMENT "warbandlib_runtime needs a Windows target: configure with --toolchain cmake/toolchain-mingw-i686.cmake"
		)
		add_dependencies(warbandlib_layout warbandlib_runtime_stub)
	endif()
endif()

# ---------------------------------------------------------------------------
# tools: the standalone injector (Windows-only executable) plus any
# Linux-native analysis/verification tools.
# ---------------------------------------------------------------------------
if(WARBANDLIB_BUILD_TOOLS)
	if(WARBANDLIB_TARGETING_WINDOWS)
		add_executable(warbandlib_injector
			"${WARBANDLIB_TOOLS_DIR}/injector/main.cpp"
		)
		target_link_libraries(warbandlib_injector
			PRIVATE
				warbandlib_project_options
				warbandlib_project_warnings
		)
		add_dependencies(warbandlib_layout warbandlib_injector)
	else()
		add_custom_target(warbandlib_tools_stub
			COMMENT "warbandlib_injector needs a Windows target: configure with --toolchain cmake/toolchain-mingw-i686.cmake"
		)
		add_dependencies(warbandlib_layout warbandlib_tools_stub)
	endif()
endif()

# ---------------------------------------------------------------------------
# examples: small, standalone, easily-disabled demo DLLs. Each one reuses
# EndSceneHook/BootstrapEndSceneHook rather than the game's own state, and
# is independent of warbandlib_runtime.dll -- to disable one, don't inject
# it (or drop its WARBANDLIB_BUILD_EXAMPLE_* option to skip building it).
# ---------------------------------------------------------------------------
if(WARBANDLIB_BUILD_EXAMPLES)
	if(WARBANDLIB_TARGETING_WINDOWS)
		if(WARBANDLIB_BUILD_EXAMPLE_OVERLAY_QUAD)
			add_library(warbandlib_example_overlay_quad SHARED
				"${WARBANDLIB_EXAMPLES_DIR}/overlay_quad/dllmain.cpp"
				"${WARBANDLIB_EXAMPLES_DIR}/overlay_quad/quad_overlay.cpp"
				"${WARBANDLIB_RUNTIME_DIR}/hooking/vtable_hook.cpp"
				"${WARBANDLIB_RUNTIME_DIR}/graphics/d3d9/endscene_hook.cpp"
				"${WARBANDLIB_RUNTIME_DIR}/graphics/d3d9/bootstrap.cpp"
			)
			target_link_libraries(warbandlib_example_overlay_quad
				PRIVATE
					warbandlib_core
					warbandlib_core_win32
			)
			set_target_properties(warbandlib_example_overlay_quad PROPERTIES
				PREFIX ""
				OUTPUT_NAME "warbandlib_example_overlay_quad"
			)
			add_custom_command(TARGET warbandlib_example_overlay_quad POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy_if_different
					"${WARBANDLIB_SIGNATURES_DIR}/mb_warband.ini"
					"$<TARGET_FILE_DIR:warbandlib_example_overlay_quad>/mb_warband.ini"
			)
			add_dependencies(warbandlib_layout warbandlib_example_overlay_quad)
		endif()
	else()
		add_custom_target(warbandlib_examples_stub
			COMMENT "Examples need a Windows target: configure with --toolchain cmake/toolchain-mingw-i686.cmake"
		)
		add_dependencies(warbandlib_layout warbandlib_examples_stub)
	endif()
endif()

# ---------------------------------------------------------------------------
# tests: host-native only (never cross-compiled). Exercises core/ logic
# (aob scanner, etc.) without needing the game or a Windows target.
# ---------------------------------------------------------------------------
if(WARBANDLIB_BUILD_TESTS AND NOT WARBANDLIB_TARGETING_WINDOWS)
	find_package(GTest QUIET)
	if(GTest_FOUND)
		enable_testing()
		include(GoogleTest)

		add_executable(warbandlib_tests
			"${WARBANDLIB_ROOT_DIR}/tests/aob_scanner_tests.cpp"
		)
		target_link_libraries(warbandlib_tests
			PRIVATE
				warbandlib_core
				GTest::gtest
				GTest::gtest_main
		)
		gtest_discover_tests(warbandlib_tests)
	else()
		message(STATUS "GTest not found -- skipping warbandlib_tests (install the 'gtest' package to enable)")
	endif()
endif()

message(STATUS "WarbandLib configuration summary")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  MSVC version: ${MSVC_VERSION}")
message(STATUS "  Targeting Windows: ${WARBANDLIB_TARGETING_WINDOWS}")
message(STATUS "  Build runtime: ${WARBANDLIB_BUILD_RUNTIME}")
message(STATUS "  Build tools: ${WARBANDLIB_BUILD_TOOLS}")
message(STATUS "  Build examples: ${WARBANDLIB_BUILD_EXAMPLES}")
message(STATUS "  Build tests: ${WARBANDLIB_BUILD_TESTS}")
message(STATUS "  Enable C ABI: ${WARBANDLIB_ENABLE_C_ABI}")
message(STATUS "  Target arch: ${WARBANDLIB_TARGET_ARCH}")

cmake_minimum_required(VERSION 3.15)

project(WarbandLib VERSION 0.1.0 LANGUAGES C CXX)

option(WARBANDLIB_BUILD_RUNTIME "Build Windows runtime and hook layer" ON)
option(WARBANDLIB_BUILD_TOOLS "Build analysis and verification tools" ON)
option(WARBANDLIB_BUILD_EXAMPLES "Build example mods and integration samples" ON)
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
endif()

add_library(warbandlib_core INTERFACE)
target_link_libraries(warbandlib_core
	INTERFACE
		warbandlib_project_options
		warbandlib_project_warnings
)

add_library(warbandlib_runtime INTERFACE)
target_link_libraries(warbandlib_runtime
	INTERFACE
		warbandlib_core
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

if(WARBANDLIB_BUILD_RUNTIME)
	add_custom_target(warbandlib_runtime_stub
		COMMENT "Runtime sources will live under runtime/ and remain MSVC 2010 oriented"
	)
	add_dependencies(warbandlib_layout warbandlib_runtime_stub)
endif()

if(WARBANDLIB_BUILD_TOOLS)
	add_custom_target(warbandlib_tools_stub
		COMMENT "Analysis and verification tools will live under tools/"
	)
	add_dependencies(warbandlib_layout warbandlib_tools_stub)
endif()

if(WARBANDLIB_BUILD_EXAMPLES)
	add_custom_target(warbandlib_examples_stub
		COMMENT "Example mods and integration samples will live under examples/"
	)
	add_dependencies(warbandlib_layout warbandlib_examples_stub)
endif()

message(STATUS "WarbandLib configuration summary")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  MSVC version: ${MSVC_VERSION}")
message(STATUS "  Build runtime: ${WARBANDLIB_BUILD_RUNTIME}")
message(STATUS "  Build tools: ${WARBANDLIB_BUILD_TOOLS}")
message(STATUS "  Build examples: ${WARBANDLIB_BUILD_EXAMPLES}")
message(STATUS "  Enable C ABI: ${WARBANDLIB_ENABLE_C_ABI}")
message(STATUS "  Target arch: ${WARBANDLIB_TARGET_ARCH}")

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

set(XWIN_DIR "$ENV{HOME}/toolchains/xwin-x86" CACHE PATH "xwin-splatted MSVC CRT + Windows SDK root")

set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER lld-link)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT llvm-mt)

set(CMAKE_C_COMPILER_TARGET i686-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET i686-pc-windows-msvc)

set(_XWIN_INCLUDE
    "/imsvc${XWIN_DIR}/crt/include"
    "/imsvc${XWIN_DIR}/sdk/include/ucrt"
    "/imsvc${XWIN_DIR}/sdk/include/um"
    "/imsvc${XWIN_DIR}/sdk/include/shared"
)
string(JOIN " " _XWIN_INCLUDE_STR ${_XWIN_INCLUDE})

set(CMAKE_C_FLAGS_INIT "${_XWIN_INCLUDE_STR}")
set(CMAKE_CXX_FLAGS_INIT "${_XWIN_INCLUDE_STR}")
set(CMAKE_C_FLAGS_DEBUG_INIT "/Z7 /Ob0 /Od")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "/Z7 /Ob0 /Od")
set(CMAKE_C_FLAGS_RELEASE_INIT "/O2 /Ob2 /DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "/O2 /Ob2 /DNDEBUG")

# xwin's splat only fetches the release CRT import/static libs (libcmt.lib,
# msvcrt.lib) unless you pass --include-debug-libs -- msvcrtd.lib/libcmtd.lib
# aren't present. CMAKE_MSVC_RUNTIME_LIBRARY (not raw /MT /MD flags, which
# CMake's own Windows-MSVC module appends after and would win) is the
# supported way to force the static release CRT for both configs, so we
# never hit CMake's default /MDd (Debug) or /MD (Release), neither of which
# we have libs for. Also matches the "statically link the CRT so the target
# machine doesn't need a redistributable" choice already made for the mingw
# build (-static-libgcc -static-libstdc++ there). Needs CMP0091=NEW, which
# a toolchain file can default on regardless of the project's
# cmake_minimum_required.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

set(_XWIN_LIBPATH
    "/libpath:${XWIN_DIR}/crt/lib/x86"
    "/libpath:${XWIN_DIR}/sdk/lib/um/x86"
    "/libpath:${XWIN_DIR}/sdk/lib/ucrt/x86"
)
string(JOIN " " _XWIN_LIBPATH_STR ${_XWIN_LIBPATH})

# UCRT implements printf/fprintf/etc. as __inline wrappers around
# __stdio_common_vfprintf in stdio.h; anything that ends up needing the
# actual exported symbol (not the inlined call) -- e.g. -O0/Debug, or just
# how clang-cl emits it here -- needs legacy_stdio_definitions.lib, which
# real MSVC links in automatically via an autolink pragma that isn't firing
# here. Link it explicitly instead of chasing that.
set(_XWIN_LIBPATH_STR "${_XWIN_LIBPATH_STR} legacy_stdio_definitions.lib")

set(CMAKE_EXE_LINKER_FLAGS_INIT "${_XWIN_LIBPATH_STR}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_XWIN_LIBPATH_STR}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_XWIN_LIBPATH_STR}")

set(CMAKE_FIND_ROOT_PATH "${XWIN_DIR}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

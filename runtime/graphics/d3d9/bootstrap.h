#pragma once

#include <windows.h>

#include <atomic>

#include "core/logging.h"
#include "runtime/graphics/d3d9/endscene_hook.h"

namespace warbandlib::runtime::graphics::d3d9 {

// Shared setup used by every EndScene-hooking DLL (the main runtime and any
// examples/ that reuse it): locates the signatures ini next to
// this_module's own file, verifies the main module's fingerprint, then
// polls (bounded, ~5 minutes) until the device pointer is available and
// hook.TryInstall() succeeds. Returns false (and logs why) on failure or if
// shutdown_requested flips true while waiting.
bool BootstrapEndSceneHook(HMODULE this_module, const char* expected_fingerprint,
                            core::Logger& logger, EndSceneHook& hook,
                            const std::atomic<bool>& shutdown_requested);

} // namespace warbandlib::runtime::graphics::d3d9

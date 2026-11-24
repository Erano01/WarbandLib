#pragma once

#include <memory>
#include <string>

#include "core/logging.h"
#include "runtime/host/event_bus.h"
#include "runtime/host/interface_registry.h"
#include "runtime/platform/hook_manager.h"
#include "runtime/platform/plugin_manager.h"
#include "warbandlib/warbandlib.h"

namespace warbandlib::runtime {

inline constexpr const char* kRuntimeVersion = "0.1.0";

struct RuntimeState {
	std::unique_ptr<core::Logger> logger;
	host::EventBus events;
	host::InterfaceRegistry interfaces;
	HookManager hooks;
	PluginManager plugins;
	wbl_game_info game_info{};
	std::string runtime_dir;
	std::string plugin_dir;
};

RuntimeState& State();

void Log(wbl_log_level level, const char* source, const char* message);
void Logf(wbl_log_level level, const char* source, const char* format, ...);

// Releases everything a plugin registered with the host.
void ReleaseOwner(const void* owner);

const wbl_host_api* HostApi();

} // namespace warbandlib::runtime

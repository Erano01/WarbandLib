#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include "core/aob_scanner.h"
#include "runtime/platform/memory_access.h"
#include "runtime/platform/overlay.h"
#include "runtime/platform/runtime_state.h"

namespace warbandlib::runtime {

RuntimeState& State() {
	static RuntimeState state;
	return state;
}

namespace {

const char* LevelName(wbl_log_level level) {
	switch (level) {
		case WBL_LOG_TRACE: return "TRACE";
		case WBL_LOG_DEBUG: return "DEBUG";
		case WBL_LOG_INFO: return "INFO";
		case WBL_LOG_WARN: return "WARN";
		default: return "ERROR";
	}
}

const char* OwnerTag(wbl_plugin self) {
	return self != nullptr && !self->id.empty() ? self->id.c_str() : "plugin";
}

void WBL_CALL CoreLog(wbl_plugin self, wbl_log_level level, const char* message) {
	Log(level, OwnerTag(self), message != nullptr ? message : "");
}

const char* WBL_CALL CoreRuntimeVersion() { return kRuntimeVersion; }

wbl_result WBL_CALL CoreGetGameInfo(wbl_game_info* out) {
	if (out == nullptr || out->struct_size < sizeof(std::uint32_t)) {
		return WBL_ERR_INVALID_ARGUMENT;
	}
	const std::uint32_t size = out->struct_size < sizeof(wbl_game_info) ? out->struct_size : sizeof(wbl_game_info);
	std::memcpy(reinterpret_cast<char*>(out) + sizeof(std::uint32_t),
	            reinterpret_cast<const char*>(&State().game_info) + sizeof(std::uint32_t), size - sizeof(std::uint32_t));
	return WBL_OK;
}

uintptr_t WBL_CALL CoreRebase(uintptr_t static_va) { return State().game_info.image_base + (static_va - 0x00400000u); }

wbl_result WBL_CALL CoreGetDataDirectory(wbl_plugin self, char* buffer, std::uint32_t buffer_size,
                                         std::uint32_t* out_len) {
	if (self == nullptr || self->id.empty()) {
		return WBL_ERR_INVALID_ARGUMENT;
	}
	const std::string dir = State().plugin_dir + self->id + "\\";
	CreateDirectoryA(dir.c_str(), nullptr);
	if (out_len != nullptr) {
		*out_len = static_cast<std::uint32_t>(dir.size());
	}
	if (buffer == nullptr || buffer_size <= dir.size()) {
		return WBL_ERR_BUFFER_TOO_SMALL;
	}
	std::memcpy(buffer, dir.c_str(), dir.size() + 1);
	return WBL_OK;
}

wbl_plugin WBL_CALL CoreFindPlugin(const char* id) {
	const wbl_plugin p = State().plugins.Find(id);
	return p != nullptr && p->status == wbl_plugin_s::Status::Loaded ? p : nullptr;
}

const wbl_plugin_info* WBL_CALL CoreGetPluginInfo(wbl_plugin plugin) {
	return plugin != nullptr && State().plugins.Owns(plugin) ? &plugin->info : nullptr;
}

wbl_result WBL_CALL CoreRegisterInterface(wbl_plugin self, const char* name, std::uint32_t version, const void* table) {
	if (self == nullptr || name == nullptr) return WBL_ERR_INVALID_ARGUMENT;
	return State().interfaces.Register(self, name, version, table);
}

const void* WBL_CALL CoreGetInterface(const char* name, std::uint32_t min_version, std::uint32_t* out_version) {
	if (name == nullptr) return nullptr;
	return State().interfaces.Get(name, min_version, out_version);
}

wbl_result WBL_CALL EventsSubscribe(wbl_plugin self, wbl_event_id id, std::int32_t priority, wbl_event_callback callback,
                                    void* user_data, wbl_subscription* out) {
	if (self == nullptr || callback == nullptr) return WBL_ERR_INVALID_ARGUMENT;
	if (!State().events.IsKnownEvent(id)) return WBL_ERR_NOT_FOUND;
	const wbl_subscription sub = State().events.Subscribe(self, id, priority, callback, user_data);
	if (sub == nullptr) return WBL_ERR_INTERNAL;
	if (out != nullptr) *out = sub;
	return WBL_OK;
}

wbl_result WBL_CALL EventsUnsubscribe(wbl_subscription subscription) {
	return State().events.Unsubscribe(subscription) ? WBL_OK : WBL_ERR_NOT_FOUND;
}

wbl_result WBL_CALL EventsRegister(wbl_plugin self, const char* name, wbl_event_flags flags, wbl_event_id* out_id) {
	if (self == nullptr || name == nullptr) return WBL_ERR_INVALID_ARGUMENT;
	return State().events.RegisterEvent(self, name, flags, out_id);
}

wbl_result WBL_CALL EventsFind(const char* name, wbl_event_id* out_id) {
	if (name == nullptr || out_id == nullptr) return WBL_ERR_INVALID_ARGUMENT;
	return State().events.FindEvent(name, out_id) ? WBL_OK : WBL_ERR_NOT_FOUND;
}

wbl_result WBL_CALL EventsPost(wbl_event_id id, const void* data, wbl_bool* out_cancelled) {
	if (id < WBL_EVENT_FIRST_CUSTOM) return WBL_ERR_INVALID_ARGUMENT; // built-in events are raised by the host only
	if (!State().events.IsKnownEvent(id)) return WBL_ERR_NOT_FOUND;
	const bool cancelled = State().events.Post(id, data);
	if (out_cancelled != nullptr) *out_cancelled = cancelled ? WBL_TRUE : WBL_FALSE;
	return WBL_OK;
}

wbl_result WBL_CALL HooksCreate(wbl_plugin self, void* target, void* detour, std::int32_t priority, void** original_slot,
                                wbl_hook* out) {
	if (self == nullptr) return WBL_ERR_INVALID_ARGUMENT;
	const wbl_result r = State().hooks.Create(self, target, detour, priority, original_slot, out);
	if (r == WBL_OK) {
		Logf(WBL_LOG_DEBUG, OwnerTag(self), "hooked 0x%08X (priority %d)",
		     static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(target)), priority);
	}
	return r;
}

wbl_result WBL_CALL HooksRemove(wbl_hook hook) { return State().hooks.Remove(hook); }

wbl_result WBL_CALL MemoryRead(uintptr_t address, void* out, std::uint32_t size) { return SafeRead(address, out, size); }
wbl_result WBL_CALL MemoryWrite(uintptr_t address, const void* data, std::uint32_t size) {
	return SafeWrite(address, data, size);
}
wbl_bool WBL_CALL MemoryIsReadable(uintptr_t address, std::uint32_t size) {
	return IsReadable(address, size) ? WBL_TRUE : WBL_FALSE;
}

wbl_result WBL_CALL MemoryFindPattern(const char* pattern, uintptr_t* out_address) {
	if (pattern == nullptr || out_address == nullptr) return WBL_ERR_INVALID_ARGUMENT;
	std::vector<core::PatternByte> parsed;
	try {
		parsed = core::ParsePattern(pattern);
	} catch (...) {
		return WBL_ERR_INVALID_ARGUMENT;
	}
	const wbl_game_info& g = State().game_info;
	const auto offset = core::Find(reinterpret_cast<const std::uint8_t*>(g.image_base), g.image_size, parsed);
	if (!offset) return WBL_ERR_NOT_FOUND;
	*out_address = g.image_base + *offset;
	return WBL_OK;
}

const wbl_core_api kCore = {
    sizeof(wbl_core_api), CoreLog,           CoreRuntimeVersion,    CoreGetGameInfo,  CoreRebase,
    CoreGetDataDirectory, CoreFindPlugin,    CoreGetPluginInfo,     CoreRegisterInterface, CoreGetInterface,
};
const wbl_events_api kEvents = {sizeof(wbl_events_api), EventsSubscribe, EventsUnsubscribe, EventsRegister, EventsFind,
                                EventsPost};
const wbl_hooks_api kHooks = {sizeof(wbl_hooks_api), HooksCreate, HooksRemove};
const wbl_memory_api kMemory = {sizeof(wbl_memory_api), MemoryRead, MemoryWrite, MemoryIsReadable, MemoryFindPattern};

} // namespace

void Log(wbl_log_level level, const char* source, const char* message) {
	core::Logger* logger = State().logger.get();
	if (logger == nullptr) return;
	std::string line = std::string("[") + LevelName(level) + "] [" + source + "] " + message;
	logger->Write(line);
}

void Logf(wbl_log_level level, const char* source, const char* format, ...) {
	char buffer[1024];
	va_list args;
	va_start(args, format);
	std::vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	Log(level, source, buffer);
}

void ReleaseOwner(const void* owner) {
	State().events.RemoveOwner(owner);
	State().hooks.RemoveOwner(owner);
	State().interfaces.RemoveOwner(owner);
}

const wbl_host_api* HostApi() {
	static const wbl_host_api host = {
	    sizeof(wbl_host_api), WBL_ABI_VERSION_MAJOR, WBL_ABI_VERSION_MINOR, &kCore, &kEvents, &kHooks, &kMemory,
	    overlay::UiApi(),     overlay::DrawApi(),
	};
	return &host;
}

} // namespace warbandlib::runtime

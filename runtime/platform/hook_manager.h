#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

#include "runtime/host/hook_chain.h"
#include "warbandlib/warbandlib.h"

namespace warbandlib::runtime {

// One MinHook hook per target whose detour is a `jmp [head]` stub, so adding or
// removing a plugin's detour only rewrites pointers: no re-patching, any calling convention.
class HookManager {
public:
	bool Initialize();
	void Shutdown();

	wbl_result Create(const void* owner, void* target, void* detour, std::int32_t priority, void** original_slot,
	                  wbl_hook* out);
	wbl_result Remove(wbl_hook hook);
	void RemoveOwner(const void* owner);
	std::size_t Count() const;

private:
	struct Target {
		void** head = nullptr;
		void* trampoline = nullptr;
		std::vector<host::ChainEntry> entries;
	};

	void Apply(Target& target);
	wbl_result RemoveLocked(std::uint64_t sequence);

	mutable std::mutex mutex_;
	std::map<void*, Target> targets_;
	std::map<std::uint64_t, void*> handles_;
	std::uint64_t next_sequence_ = 1;
	bool initialized_ = false;
};

} // namespace warbandlib::runtime

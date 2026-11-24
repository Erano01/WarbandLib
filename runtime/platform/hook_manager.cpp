#include "runtime/platform/hook_manager.h"

#include <windows.h>

#include <cstring>

#include "MinHook.h"
#include "runtime/platform/runtime_state.h"

namespace warbandlib::runtime {

namespace {

std::uint8_t* MakeJumpStub(void** head) {
	auto* stub = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	if (stub == nullptr) {
		return nullptr;
	}
	stub[0] = 0xFF; // jmp dword ptr [head]
	stub[1] = 0x25;
	const auto address = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(head));
	std::memcpy(stub + 2, &address, sizeof(address));
	DWORD old = 0;
	VirtualProtect(stub, 16, PAGE_EXECUTE_READ, &old);
	FlushInstructionCache(GetCurrentProcess(), stub, 16);
	return stub;
}

} // namespace

bool HookManager::Initialize() {
	std::lock_guard<std::mutex> lock(mutex_);
	const MH_STATUS status = MH_Initialize();
	initialized_ = status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED;
	return initialized_;
}

void HookManager::Shutdown() {
	std::lock_guard<std::mutex> lock(mutex_);
	if (!initialized_) {
		return;
	}
	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
	targets_.clear();
	handles_.clear();
	initialized_ = false;
}

void HookManager::Apply(Target& target) {
	const host::ChainLayout layout = host::ComputeChainLayout(target.entries, target.trampoline);
	for (const auto& [slot, value] : layout.slot_writes) {
		*slot = value;
	}
	InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(target.head), layout.head);
}

wbl_result HookManager::Create(const void* owner, void* target_fn, void* detour, std::int32_t priority,
                               void** original_slot, wbl_hook* out) {
	if (target_fn == nullptr || detour == nullptr || original_slot == nullptr) {
		return WBL_ERR_INVALID_ARGUMENT;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	if (!initialized_) {
		return WBL_ERR_WRONG_CONTEXT;
	}

	auto it = targets_.find(target_fn);
	if (it != targets_.end()) {
		for (const host::ChainEntry& e : it->second.entries) {
			if (e.detour == detour || e.original_slot == original_slot) {
				return WBL_ERR_ALREADY_EXISTS;
			}
		}
	}

	const std::uint64_t sequence = next_sequence_++;
	const host::ChainEntry entry{owner, detour, original_slot, priority, sequence};

	if (it == targets_.end()) {
		Target target;
		target.head = new void*(detour); // stub and head are never freed: a thread may still be inside them
		*original_slot = nullptr;
		std::uint8_t* stub = MakeJumpStub(target.head);
		if (stub == nullptr) {
			return WBL_ERR_INTERNAL;
		}
		void* trampoline = nullptr;
		MH_STATUS status = MH_CreateHook(target_fn, stub, &trampoline);
		if (status != MH_OK) {
			Logf(WBL_LOG_ERROR, "hooks", "MH_CreateHook(0x%08X) failed: %s",
			     static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(target_fn)), MH_StatusToString(status));
			return WBL_ERR_HOOK_FAILED;
		}
		target.trampoline = trampoline;
		target.entries.push_back(entry);
		Apply(target);
		status = MH_EnableHook(target_fn);
		if (status != MH_OK) {
			MH_RemoveHook(target_fn);
			Logf(WBL_LOG_ERROR, "hooks", "MH_EnableHook(0x%08X) failed: %s",
			     static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(target_fn)), MH_StatusToString(status));
			return WBL_ERR_HOOK_FAILED;
		}
		targets_.emplace(target_fn, std::move(target));
	} else {
		it->second.entries.push_back(entry);
		host::SortChain(it->second.entries);
		Apply(it->second);
	}

	handles_[sequence] = target_fn;
	if (out != nullptr) {
		*out = reinterpret_cast<wbl_hook>(static_cast<std::uintptr_t>(sequence));
	}
	return WBL_OK;
}

wbl_result HookManager::RemoveLocked(std::uint64_t sequence) {
	const auto h = handles_.find(sequence);
	if (h == handles_.end()) {
		return WBL_ERR_NOT_FOUND;
	}
	void* target_fn = h->second;
	handles_.erase(h);

	Target& target = targets_.at(target_fn);
	for (auto e = target.entries.begin(); e != target.entries.end(); ++e) {
		if (e->sequence == sequence) {
			target.entries.erase(e);
			break;
		}
	}
	if (target.entries.empty()) {
		MH_DisableHook(target_fn);
		MH_RemoveHook(target_fn);
		targets_.erase(target_fn);
	} else {
		Apply(target);
	}
	return WBL_OK;
}

wbl_result HookManager::Remove(wbl_hook hook) {
	std::lock_guard<std::mutex> lock(mutex_);
	return RemoveLocked(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(hook)));
}

void HookManager::RemoveOwner(const void* owner) {
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<std::uint64_t> owned;
	for (const auto& [target_fn, target] : targets_) {
		for (const host::ChainEntry& e : target.entries) {
			if (e.owner == owner) owned.push_back(e.sequence);
		}
	}
	for (const std::uint64_t seq : owned) {
		RemoveLocked(seq);
	}
}

std::size_t HookManager::Count() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return handles_.size();
}

} // namespace warbandlib::runtime

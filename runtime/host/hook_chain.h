#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace warbandlib::host {

struct ChainEntry {
	const void* owner;
	void* detour;
	void** original_slot;
	std::int32_t priority;
	std::uint64_t sequence;
};

// Highest priority first (outermost detour), then registration order.
void SortChain(std::vector<ChainEntry>& entries);

struct ChainLayout {
	void* head = nullptr;
	// Applied in order: innermost first, so every pointer is valid before it is linked in.
	std::vector<std::pair<void**, void*>> slot_writes;
};

ChainLayout ComputeChainLayout(const std::vector<ChainEntry>& sorted_entries, void* trampoline);

} // namespace warbandlib::host

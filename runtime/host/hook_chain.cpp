#include "runtime/host/hook_chain.h"

#include <algorithm>

namespace warbandlib::host {

void SortChain(std::vector<ChainEntry>& entries) {
	std::stable_sort(entries.begin(), entries.end(), [](const ChainEntry& a, const ChainEntry& b) {
		if (a.priority != b.priority) return a.priority > b.priority;
		return a.sequence < b.sequence;
	});
}

ChainLayout ComputeChainLayout(const std::vector<ChainEntry>& sorted_entries, void* trampoline) {
	ChainLayout layout;
	if (sorted_entries.empty()) {
		layout.head = trampoline;
		return layout;
	}
	for (std::size_t i = sorted_entries.size(); i-- > 0;) {
		void* next = (i + 1 < sorted_entries.size()) ? sorted_entries[i + 1].detour : trampoline;
		layout.slot_writes.emplace_back(sorted_entries[i].original_slot, next);
	}
	layout.head = sorted_entries.front().detour;
	return layout;
}

} // namespace warbandlib::host

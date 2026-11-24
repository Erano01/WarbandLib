#include <gtest/gtest.h>

#include "runtime/host/hook_chain.h"

using namespace warbandlib::host;

namespace {
void* P(std::uintptr_t v) { return reinterpret_cast<void*>(v); }
} // namespace

TEST(HookChain, SortsByPriorityThenSequence) {
	void* slots[3] = {};
	std::vector<ChainEntry> e = {
	    {nullptr, P(0xA), &slots[0], 0, 1},
	    {nullptr, P(0xB), &slots[1], 100, 2},
	    {nullptr, P(0xC), &slots[2], 0, 0},
	};
	SortChain(e);
	EXPECT_EQ(e[0].detour, P(0xB));
	EXPECT_EQ(e[1].detour, P(0xC));
	EXPECT_EQ(e[2].detour, P(0xA));
}

TEST(HookChain, LinksOutermostToTrampoline) {
	void* slots[3] = {};
	std::vector<ChainEntry> e = {
	    {nullptr, P(0x10), &slots[0], 9, 0},
	    {nullptr, P(0x20), &slots[1], 5, 1},
	    {nullptr, P(0x30), &slots[2], 1, 2},
	};
	const ChainLayout layout = ComputeChainLayout(e, P(0x99));
	EXPECT_EQ(layout.head, P(0x10));
	ASSERT_EQ(layout.slot_writes.size(), 3u);
	EXPECT_EQ(layout.slot_writes[0].first, &slots[2]); // innermost written first
	for (const auto& [slot, value] : layout.slot_writes) *slot = value;
	EXPECT_EQ(slots[0], P(0x20));
	EXPECT_EQ(slots[1], P(0x30));
	EXPECT_EQ(slots[2], P(0x99));
}

TEST(HookChain, EmptyChainFallsThrough) {
	const ChainLayout layout = ComputeChainLayout({}, P(0x99));
	EXPECT_EQ(layout.head, P(0x99));
	EXPECT_TRUE(layout.slot_writes.empty());
}

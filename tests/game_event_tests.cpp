#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "runtime/host/event_bus.h"
#include "runtime/host/game_event_flow.h"
#include "runtime/host/symbol_table.h"
#include "runtime/host/task_queue.h"

using namespace warbandlib::host;

namespace {

struct Hit {
	int damage;
};

wbl_event_reply WBL_CALL DoubleDamage(const wbl_event* e, void*) {
	static_cast<Hit*>(const_cast<void*>(e->data))->damage *= 2;
	return WBL_EVENT_CONTINUE;
}
wbl_event_reply WBL_CALL Cancel(const wbl_event*, void*) { return WBL_EVENT_CANCEL; }
wbl_event_reply WBL_CALL CountCalls(const wbl_event*, void* user) {
	++*static_cast<int*>(user);
	return WBL_EVENT_CANCEL; // ignored on POST: not cancellable
}

} // namespace

TEST(GameEvents, PhaseIdsAreKnownAndOnlyPreCancels) {
	EventBus bus;
	for (const wbl_event_id base : kGameEvents) {
		EXPECT_TRUE(bus.IsKnownEvent(WBL_PRE(base)));
		EXPECT_TRUE(bus.IsKnownEvent(WBL_POST(base)));
		EXPECT_FALSE(bus.IsKnownEvent(base)); // a phase must be chosen
		EXPECT_EQ(WBL_EVENT_BASE(WBL_PRE(base)), base);
		EXPECT_EQ(WBL_EVENT_PHASE(WBL_POST(base)), WBL_PHASE_POST);
	}
	int posts = 0;
	bus.Subscribe(nullptr, WBL_POST(WBL_EVENT_AGENT_HIT), 0, CountCalls, &posts);
	bus.Subscribe(nullptr, WBL_POST(WBL_EVENT_AGENT_HIT), 0, CountCalls, &posts);
	EXPECT_FALSE(bus.Post(WBL_POST(WBL_EVENT_AGENT_HIT), nullptr));
	EXPECT_EQ(posts, 2);
}

TEST(GameEvents, RunPhasedOrderAndPayloadChanges) {
	EventBus bus;
	Hit hit{10};
	int original_saw = 0;
	bus.Subscribe(nullptr, WBL_PRE(WBL_EVENT_AGENT_HIT), 0, DoubleDamage, nullptr);
	bus.Subscribe(nullptr, WBL_POST(WBL_EVENT_AGENT_HIT), 0, DoubleDamage, nullptr);
	EXPECT_TRUE(RunPhased(bus, WBL_EVENT_AGENT_HIT, &hit, [&] { original_saw = hit.damage; }));
	EXPECT_EQ(original_saw, 20);
	EXPECT_EQ(hit.damage, 40);
}

TEST(GameEvents, PreCancelSkipsOriginalAndPost) {
	EventBus bus;
	Hit hit{1};
	int posts = 0;
	bool ran = false;
	bus.Subscribe(nullptr, WBL_PRE(WBL_EVENT_SCRIPT_CALL), 0, Cancel, nullptr);
	bus.Subscribe(nullptr, WBL_POST(WBL_EVENT_SCRIPT_CALL), 0, CountCalls, &posts);
	EXPECT_TRUE(AnySubscribers(bus, WBL_EVENT_SCRIPT_CALL));
	EXPECT_FALSE(AnySubscribers(bus, WBL_EVENT_OPERATION_BLOCK));
	EXPECT_FALSE(RunPhased(bus, WBL_EVENT_SCRIPT_CALL, &hit, [&] { ran = true; }));
	EXPECT_FALSE(ran);
	EXPECT_EQ(posts, 0);
}

TEST(SymbolTable, PicksTheSectionOfTheRunningExe) {
	std::istringstream ini(
	    "; comment\n[aaaa]\nx = 1\n[ABCD]\nEvents.Some_Function = 0x123456 ; trailing\nplain = 42\nbad = zz\n");
	const auto t = SymbolTable::Parse(ini, "abcd");
	ASSERT_TRUE(t.has_value());
	EXPECT_EQ(t->Get("events.some_function"), 0x123456u);
	EXPECT_EQ(t->Get("PLAIN"), 42u);
	EXPECT_FALSE(t->Has("bad"));
	EXPECT_FALSE(t->Has("x"));
	std::istringstream other("[ffff]\nx=1\n");
	EXPECT_FALSE(SymbolTable::Parse(other, "abcd").has_value());
}

namespace {
std::string g_order;
void WBL_CALL AppendA(void*) { g_order += 'a'; }
void WBL_CALL AppendB(void*) { g_order += 'b'; }
TaskQueue* g_queue = nullptr;
void WBL_CALL Requeue(void*) {
	g_order += 'r';
	g_queue->Push(nullptr, AppendA, nullptr);
}
} // namespace

TEST(TaskQueue, DrainsInOrderAndDefersTasksQueuedWhileDraining) {
	TaskQueue q;
	g_queue = &q;
	g_order.clear();
	int owner = 0;
	q.Push(nullptr, AppendA, nullptr);
	q.Push(&owner, AppendB, nullptr);
	q.Push(nullptr, Requeue, nullptr);
	q.RemoveOwner(&owner);
	EXPECT_EQ(q.Drain(), 2u);
	EXPECT_EQ(g_order, "ar");
	EXPECT_EQ(q.Pending(), 1u);
	EXPECT_EQ(q.Drain(), 1u);
	EXPECT_EQ(g_order, "ara");
}

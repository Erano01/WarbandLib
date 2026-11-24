#include <gtest/gtest.h>

#include <string>

#include "runtime/host/event_bus.h"

using warbandlib::host::EventBus;

namespace {

struct Recorder {
	std::string calls;
	char tag;
	wbl_event_reply reply = WBL_EVENT_CONTINUE;
};

wbl_event_reply WBL_CALL Record(const wbl_event* event, void* user) {
	auto* r = static_cast<Recorder*>(user);
	r->calls += r->tag;
	EXPECT_EQ(event->struct_size, sizeof(wbl_event));
	return r->reply;
}

std::string g_log;
struct Tagged {
	char tag;
	wbl_event_reply reply;
};
wbl_event_reply WBL_CALL Append(const wbl_event*, void* user) {
	auto* t = static_cast<Tagged*>(user);
	g_log += t->tag;
	return t->reply;
}

} // namespace

TEST(EventBus, PriorityThenRegistrationOrder) {
	EventBus bus;
	g_log.clear();
	Tagged a{'a', WBL_EVENT_CONTINUE}, b{'b', WBL_EVENT_CONTINUE}, c{'c', WBL_EVENT_CONTINUE}, d{'d', WBL_EVENT_CONTINUE};
	bus.Subscribe(nullptr, WBL_EVENT_FRAME, 0, Append, &a);
	bus.Subscribe(nullptr, WBL_EVENT_FRAME, 10, Append, &b);
	bus.Subscribe(nullptr, WBL_EVENT_FRAME, 0, Append, &c);
	bus.Subscribe(nullptr, WBL_EVENT_FRAME, -5, Append, &d);
	bus.Post(WBL_EVENT_FRAME, nullptr);
	EXPECT_EQ(g_log, "bacd");
}

TEST(EventBus, UnknownEventsAreRefused) {
	EventBus bus;
	Recorder r{"", 'x'};
	EXPECT_EQ(bus.Subscribe(nullptr, 999, 0, Record, &r), nullptr);
	EXPECT_FALSE(bus.IsKnownEvent(999));
	EXPECT_TRUE(bus.IsKnownEvent(WBL_EVENT_UI));
}

TEST(EventBus, CancelOnlyStopsCancellableEvents) {
	EventBus bus;
	g_log.clear();
	wbl_event_id cancellable = 0, plain = 0;
	ASSERT_EQ(bus.RegisterEvent(nullptr, "test.cancellable", WBL_EVENT_FLAG_CANCELLABLE, &cancellable), WBL_OK);
	ASSERT_EQ(bus.RegisterEvent(nullptr, "test.plain", 0, &plain), WBL_OK);
	EXPECT_GE(cancellable, WBL_EVENT_FIRST_CUSTOM);
	Tagged stop{'s', WBL_EVENT_CANCEL}, after{'a', WBL_EVENT_CONTINUE};
	for (const wbl_event_id id : {cancellable, plain}) {
		bus.Subscribe(nullptr, id, 1, Append, &stop);
		bus.Subscribe(nullptr, id, 0, Append, &after);
	}
	EXPECT_TRUE(bus.Post(cancellable, nullptr));
	EXPECT_EQ(g_log, "s");
	g_log.clear();
	EXPECT_FALSE(bus.Post(plain, nullptr));
	EXPECT_EQ(g_log, "sa");
}

TEST(EventBus, RegisterIsIdempotentByName) {
	EventBus bus;
	wbl_event_id first = 0, second = 0, found = 0;
	ASSERT_EQ(bus.RegisterEvent(nullptr, "x.y", 0, &first), WBL_OK);
	ASSERT_EQ(bus.RegisterEvent(nullptr, "x.y", 0, &second), WBL_OK);
	EXPECT_EQ(first, second);
	EXPECT_EQ(bus.RegisterEvent(nullptr, "x.y", WBL_EVENT_FLAG_CANCELLABLE, &second), WBL_ERR_ALREADY_EXISTS);
	ASSERT_TRUE(bus.FindEvent("x.y", &found));
	EXPECT_EQ(found, first);
	EXPECT_FALSE(bus.FindEvent("nope", &found));
}

TEST(EventBus, UnsubscribeAndRemoveOwner) {
	EventBus bus;
	int owner_a = 0, owner_b = 0;
	Recorder r1{"", '1'}, r2{"", '2'};
	const wbl_subscription s1 = bus.Subscribe(&owner_a, WBL_EVENT_DRAW, 0, Record, &r1);
	bus.Subscribe(&owner_b, WBL_EVENT_DRAW, 0, Record, &r2);
	bus.Subscribe(&owner_b, WBL_EVENT_UI, 0, Record, &r2);
	EXPECT_TRUE(bus.Unsubscribe(s1));
	EXPECT_FALSE(bus.Unsubscribe(s1));
	bus.RemoveOwner(&owner_b);
	bus.Post(WBL_EVENT_DRAW, nullptr);
	bus.Post(WBL_EVENT_UI, nullptr);
	EXPECT_EQ(r1.calls, "");
	EXPECT_EQ(r2.calls, "");
	EXPECT_EQ(bus.SubscriberCount(WBL_EVENT_DRAW), 0u);
}

namespace {
EventBus* g_bus = nullptr;
wbl_subscription g_victim = nullptr;
wbl_event_reply WBL_CALL KillOther(const wbl_event*, void*) {
	g_bus->Unsubscribe(g_victim);
	return WBL_EVENT_CONTINUE;
}
} // namespace

TEST(EventBus, UnsubscribeDuringDispatchSkipsTheRemovedCallback) {
	EventBus bus;
	g_bus = &bus;
	Recorder victim{"", 'v'};
	bus.Subscribe(nullptr, WBL_EVENT_FRAME, 10, KillOther, nullptr);
	g_victim = bus.Subscribe(nullptr, WBL_EVENT_FRAME, 0, Record, &victim);
	bus.Post(WBL_EVENT_FRAME, nullptr);
	EXPECT_EQ(victim.calls, "");
}

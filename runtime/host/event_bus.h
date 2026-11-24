#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "warbandlib/warbandlib.h"

namespace warbandlib::host {

// Base ids of the built-in game events; each exists as WBL_PRE(id) (cancellable) and WBL_POST(id).
inline constexpr wbl_event_id kGameEvents[] = {WBL_EVENT_MODULE_TRIGGER, WBL_EVENT_SCRIPT_CALL,
                                               WBL_EVENT_OPERATION_BLOCK, WBL_EVENT_AGENT_HIT};

class EventBus {
public:
	EventBus();

	wbl_subscription Subscribe(const void* owner, wbl_event_id id, std::int32_t priority, wbl_event_callback callback,
	                           void* user_data);
	bool Unsubscribe(wbl_subscription subscription);
	void RemoveOwner(const void* owner);

	wbl_result RegisterEvent(const void* owner, const std::string& name, wbl_event_flags flags, wbl_event_id* out_id);
	bool FindEvent(const std::string& name, wbl_event_id* out_id) const;
	bool IsKnownEvent(wbl_event_id id) const;

	// Returns true when a subscriber cancelled a cancellable event.
	bool Post(wbl_event_id id, const void* data);

	std::size_t SubscriberCount(wbl_event_id id) const;
	bool HasSubscribers(wbl_event_id id) const;

private:
	struct Subscription {
		const void* owner;
		wbl_event_id id;
		std::int32_t priority;
		std::uint64_t sequence;
		wbl_event_callback callback;
		void* user_data;
		std::atomic<bool> alive{true};
	};
	using SubscriptionPtr = std::shared_ptr<Subscription>;

	struct CustomEvent {
		wbl_event_id id;
		wbl_event_flags flags;
		const void* owner;
	};

	mutable std::mutex mutex_;
	std::map<wbl_event_id, std::vector<SubscriptionPtr>> subscribers_;
	std::map<std::string, CustomEvent> custom_by_name_;
	std::map<wbl_event_id, wbl_event_flags> flags_;
	wbl_event_id next_custom_id_ = WBL_EVENT_FIRST_CUSTOM;
	std::uint64_t next_sequence_ = 0;
};

} // namespace warbandlib::host

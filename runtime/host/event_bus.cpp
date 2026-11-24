#include "runtime/host/event_bus.h"

#include <algorithm>

namespace warbandlib::host {

EventBus::EventBus() {
	for (wbl_event_id id = WBL_EVENT_FRAME; id <= WBL_EVENT_SHUTDOWN; ++id) {
		flags_[id] = 0;
	}
}

wbl_subscription EventBus::Subscribe(const void* owner, wbl_event_id id, std::int32_t priority,
                                     wbl_event_callback callback, void* user_data) {
	if (callback == nullptr) {
		return nullptr;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	if (flags_.find(id) == flags_.end()) {
		return nullptr;
	}
	auto sub = std::make_shared<Subscription>();
	sub->owner = owner;
	sub->id = id;
	sub->priority = priority;
	sub->sequence = next_sequence_++;
	sub->callback = callback;
	sub->user_data = user_data;

	std::vector<SubscriptionPtr>& list = subscribers_[id];
	const auto pos = std::upper_bound(list.begin(), list.end(), sub, [](const SubscriptionPtr& a, const SubscriptionPtr& b) {
		if (a->priority != b->priority) return a->priority > b->priority;
		return a->sequence < b->sequence;
	});
	list.insert(pos, sub);
	return reinterpret_cast<wbl_subscription>(sub.get());
}

bool EventBus::Unsubscribe(wbl_subscription subscription) {
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto& [id, list] : subscribers_) {
		for (auto it = list.begin(); it != list.end(); ++it) {
			if (reinterpret_cast<wbl_subscription>(it->get()) == subscription) {
				(*it)->alive.store(false);
				list.erase(it);
				return true;
			}
		}
	}
	return false;
}

void EventBus::RemoveOwner(const void* owner) {
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto& [id, list] : subscribers_) {
		list.erase(std::remove_if(list.begin(), list.end(),
		                          [owner](const SubscriptionPtr& s) {
			                          if (s->owner != owner) return false;
			                          s->alive.store(false);
			                          return true;
		                          }),
		           list.end());
	}
}

wbl_result EventBus::RegisterEvent(const void* owner, const std::string& name, wbl_event_flags flags,
                                   wbl_event_id* out_id) {
	if (name.empty() || out_id == nullptr) {
		return WBL_ERR_INVALID_ARGUMENT;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = custom_by_name_.find(name);
	if (it != custom_by_name_.end()) {
		*out_id = it->second.id;
		return it->second.flags == flags ? WBL_OK : WBL_ERR_ALREADY_EXISTS;
	}
	const wbl_event_id id = next_custom_id_++;
	custom_by_name_.emplace(name, CustomEvent{id, flags, owner});
	flags_[id] = flags;
	*out_id = id;
	return WBL_OK;
}

bool EventBus::FindEvent(const std::string& name, wbl_event_id* out_id) const {
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = custom_by_name_.find(name);
	if (it == custom_by_name_.end()) {
		return false;
	}
	*out_id = it->second.id;
	return true;
}

bool EventBus::IsKnownEvent(wbl_event_id id) const {
	std::lock_guard<std::mutex> lock(mutex_);
	return flags_.find(id) != flags_.end();
}

bool EventBus::Post(wbl_event_id id, const void* data) {
	std::vector<SubscriptionPtr> snapshot;
	wbl_event_flags flags = 0;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		const auto f = flags_.find(id);
		if (f == flags_.end()) {
			return false;
		}
		flags = f->second;
		const auto it = subscribers_.find(id);
		if (it == subscribers_.end() || it->second.empty()) {
			return false;
		}
		snapshot = it->second;
	}

	wbl_event event{};
	event.struct_size = sizeof(event);
	event.id = id;
	event.flags = flags;
	event.data = data;
	const bool cancellable = (flags & WBL_EVENT_FLAG_CANCELLABLE) != 0;
	for (const SubscriptionPtr& sub : snapshot) {
		if (!sub->alive.load()) {
			continue;
		}
		const wbl_event_reply reply = sub->callback(&event, sub->user_data);
		if (cancellable && reply == WBL_EVENT_CANCEL) {
			return true;
		}
	}
	return false;
}

std::size_t EventBus::SubscriberCount(wbl_event_id id) const {
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = subscribers_.find(id);
	return it == subscribers_.end() ? 0 : it->second.size();
}

} // namespace warbandlib::host

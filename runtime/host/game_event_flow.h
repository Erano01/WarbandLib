#pragma once

#include "runtime/host/event_bus.h"

namespace warbandlib::host {

// PRE, then the game's own code unless a PRE handler cancelled, then POST.
// Returns false when PRE cancelled (the original did not run and POST was not raised).
template <typename Original>
bool RunPhased(EventBus& bus, wbl_event_id base, void* payload, Original&& original) {
	if (bus.Post(WBL_PRE(base), payload)) {
		return false;
	}
	original();
	bus.Post(WBL_POST(base), payload);
	return true;
}

inline bool AnySubscribers(const EventBus& bus, wbl_event_id base) {
	return bus.HasSubscribers(WBL_PRE(base)) || bus.HasSubscribers(WBL_POST(base));
}

} // namespace warbandlib::host

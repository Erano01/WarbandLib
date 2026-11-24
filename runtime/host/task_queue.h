#pragma once

#include <cstddef>
#include <deque>
#include <mutex>

#include "warbandlib/warbandlib.h"

namespace warbandlib::host {

// Work handed from any thread to the game thread. Push from anywhere, Drain on the game thread.
class TaskQueue {
public:
	void Push(const void* owner, wbl_task_fn fn, void* user_data);
	// Runs every task queued before the call, in order; tasks queued meanwhile wait for the next Drain.
	std::size_t Drain();
	// Drops the owner's pending tasks (plugin unload).
	void RemoveOwner(const void* owner);
	std::size_t Pending() const;

private:
	struct Task {
		const void* owner;
		wbl_task_fn fn;
		void* user_data;
	};
	mutable std::mutex mutex_;
	std::deque<Task> tasks_;
};

} // namespace warbandlib::host

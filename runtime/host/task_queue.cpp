#include "runtime/host/task_queue.h"

#include <algorithm>

namespace warbandlib::host {

void TaskQueue::Push(const void* owner, wbl_task_fn fn, void* user_data) {
	std::lock_guard<std::mutex> lock(mutex_);
	tasks_.push_back(Task{owner, fn, user_data});
}

std::size_t TaskQueue::Drain() {
	std::deque<Task> batch;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		batch.swap(tasks_);
	}
	for (const Task& t : batch) t.fn(t.user_data);
	return batch.size();
}

void TaskQueue::RemoveOwner(const void* owner) {
	std::lock_guard<std::mutex> lock(mutex_);
	tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(), [owner](const Task& t) { return t.owner == owner; }),
	             tasks_.end());
}

std::size_t TaskQueue::Pending() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return tasks_.size();
}

} // namespace warbandlib::host

#include "core/logging.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <mutex>

namespace warbandlib::core {

namespace {
std::mutex g_log_mutex;
}

Logger::Logger(std::string log_file_path) : log_file_path_(std::move(log_file_path)) {}

void Logger::Write(std::string_view message) {
	const auto now = std::chrono::system_clock::now();
	const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

	std::tm tm_buf{};
#if defined(_WIN32)
	localtime_s(&tm_buf, &now_time);
#else
	localtime_r(&now_time, &tm_buf);
#endif

	char timestamp[32];
	std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

	std::lock_guard<std::mutex> lock(g_log_mutex);
	std::ofstream out(log_file_path_, std::ios::app);
	if (!out.is_open()) {
		return;
	}
	out << '[' << timestamp << "] " << message << '\n';
}

} // namespace warbandlib::core

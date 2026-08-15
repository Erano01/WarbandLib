#pragma once

#include <string>
#include <string_view>

namespace warbandlib::core {

// Append-only, thread-safe file logger. Opens lazily on first Write() call
// and stays open for the process lifetime (closed by the OS on unload).
class Logger {
public:
	explicit Logger(std::string log_file_path);

	void Write(std::string_view message);

private:
	std::string log_file_path_;
};

} // namespace warbandlib::core

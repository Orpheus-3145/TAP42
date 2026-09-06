#include "logging/logger.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace {

std::mutex log_mutex;

std::string timestamp_iso8601() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return buf;
}

std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        if (c == '\n') { out += "\\n"; continue; }
        out.push_back(c);
    }
    return out;
}

void write_log(const char* level, const std::string& event, const LogFields& fields) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::printf("{\"ts\":\"%s\",\"level\":\"%s\",\"event\":\"%s\"", timestamp_iso8601().c_str(),
                level, escape_json(event).c_str());
    for (auto& [key, value] : fields) {
        std::printf(",\"%s\":\"%s\"", escape_json(key).c_str(), escape_json(value).c_str());
    }
    std::printf("}\n");
    std::fflush(stdout);
}

} // namespace

void log_info(const std::string& event, const LogFields& fields) { write_log("INFO", event, fields); }
void log_warn(const std::string& event, const LogFields& fields) { write_log("WARN", event, fields); }
void log_error(const std::string& event, const LogFields& fields) { write_log("ERROR", event, fields); }

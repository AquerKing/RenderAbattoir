#pragma once

#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace logsys {

enum class Level { Info, Warning, Error };

struct Entry {
    std::string channel;
    Level level = Level::Info;
    std::string message;
};

// An application-wide, thread-safe log store. Registering a channel is
// optional for logging, but provides a central discoverable channel catalog.
class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void registerChannel(std::string channel) {
        std::scoped_lock lock(mutex_);
        channels_.insert(std::move(channel));
    }

    void write(std::string_view channel, Level level, std::string message) {
        std::scoped_lock lock(mutex_);
        channels_.insert(std::string(channel));
        entries_.push_back({std::string(channel), level, std::move(message)});
    }

    void info(std::string_view channel, std::string message) { write(channel, Level::Info, std::move(message)); }
    void warning(std::string_view channel, std::string message) { write(channel, Level::Warning, std::move(message)); }
    void error(std::string_view channel, std::string message) { write(channel, Level::Error, std::move(message)); }

    std::vector<Entry> entries() const {
        std::scoped_lock lock(mutex_);
        return entries_;
    }

    std::vector<std::string> channels() const {
        std::scoped_lock lock(mutex_);
        std::vector<std::string> result{channels_.begin(), channels_.end()};
        std::sort(result.begin(), result.end());
        return result;
    }

    void clear() {
        std::scoped_lock lock(mutex_);
        entries_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_set<std::string> channels_;
    std::vector<Entry> entries_;
};

} // namespace logsys

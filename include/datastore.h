#pragma once

#include <chrono>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace redisclone {

class DataStore {
public:
    using Clock = std::chrono::system_clock;

    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    bool expire(const std::string& key, std::chrono::seconds ttl);

    std::vector<std::string> keys();
    std::size_t keyCount();
    std::size_t removeExpiredKeys();
    std::size_t estimateMemoryBytes();

    bool save(const std::filesystem::path& path, std::string* error = nullptr);
    bool load(const std::filesystem::path& path, std::string* error = nullptr);

private:
    struct Entry {
        std::string value;
        std::optional<Clock::time_point> expires_at;
    };

    std::unordered_map<std::string, Entry> data_;
    mutable std::mutex mutex_;

    static bool isExpired(const Entry& entry, Clock::time_point now);
    void removeExpiredLocked(Clock::time_point now);
};

} // namespace redisclone


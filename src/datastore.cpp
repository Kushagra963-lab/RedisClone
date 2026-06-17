#include "datastore.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace redisclone {

void DataStore::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_[key] = Entry{value, std::nullopt};
}

std::optional<std::string> DataStore::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = Clock::now();
    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }
    if (isExpired(it->second, now)) {
        data_.erase(it);
        return std::nullopt;
    }
    return it->second.value;
}

bool DataStore::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.erase(key) > 0;
}

bool DataStore::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = Clock::now();
    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }
    if (isExpired(it->second, now)) {
        data_.erase(it);
        return false;
    }
    return true;
}

bool DataStore::expire(const std::string& key, std::chrono::seconds ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = Clock::now();
    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }
    if (isExpired(it->second, now)) {
        data_.erase(it);
        return false;
    }
    it->second.expires_at = now + ttl;
    return true;
}

std::vector<std::string> DataStore::keys() {
    std::lock_guard<std::mutex> lock(mutex_);
    removeExpiredLocked(Clock::now());

    std::vector<std::string> result;
    result.reserve(data_.size());
    for (const auto& [key, entry] : data_) {
        (void)entry;
        result.push_back(key);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::size_t DataStore::keyCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    removeExpiredLocked(Clock::now());
    return data_.size();
}

std::size_t DataStore::removeExpiredKeys() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto before = data_.size();
    removeExpiredLocked(Clock::now());
    return before - data_.size();
}

std::size_t DataStore::estimateMemoryBytes() {
    std::lock_guard<std::mutex> lock(mutex_);
    removeExpiredLocked(Clock::now());

    std::size_t bytes = 0;
    for (const auto& [key, entry] : data_) {
        bytes += key.capacity();
        bytes += entry.value.capacity();
        bytes += sizeof(Entry);
    }
    bytes += data_.bucket_count() * sizeof(void*);
    return bytes;
}

bool DataStore::save(const std::filesystem::path& path, std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = Clock::now();
    removeExpiredLocked(now);

    try {
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream out(path, std::ios::trunc);
        if (!out) {
            if (error) {
                *error = "could not open " + path.string();
            }
            return false;
        }

        for (const auto& [key, entry] : data_) {
            long long expiry_ms = 0;
            if (entry.expires_at) {
                expiry_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                entry.expires_at->time_since_epoch())
                                .count();
            }
            out << expiry_ms << ' ' << std::quoted(key) << ' ' << std::quoted(entry.value) << '\n';
        }
    } catch (const std::exception& ex) {
        if (error) {
            *error = ex.what();
        }
        return false;
    }

    return true;
}

bool DataStore::load(const std::filesystem::path& path, std::string* error) {
    if (!std::filesystem::exists(path)) {
        return true;
    }

    std::ifstream in(path);
    if (!in) {
        if (error) {
            *error = "could not open " + path.string();
        }
        return false;
    }

    std::unordered_map<std::string, Entry> loaded;
    std::string line;
    std::size_t line_number = 0;
    const auto now = Clock::now();

    while (std::getline(in, line)) {
        ++line_number;
        if (line.empty()) {
            continue;
        }

        std::istringstream row(line);
        long long expiry_ms = 0;
        std::string key;
        std::string value;

        if (row >> expiry_ms >> std::quoted(key) >> std::quoted(value)) {
            std::optional<Clock::time_point> expires_at;
            if (expiry_ms > 0) {
                expires_at = Clock::time_point{std::chrono::milliseconds(expiry_ms)};
                if (*expires_at <= now) {
                    continue;
                }
            }
            loaded[key] = Entry{value, expires_at};
            continue;
        }

        const auto separator = line.find('=');
        if (separator != std::string::npos) {
            loaded[line.substr(0, separator)] = Entry{line.substr(separator + 1), std::nullopt};
            continue;
        }

        if (error) {
            *error = "invalid dump format at line " + std::to_string(line_number);
        }
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    data_ = std::move(loaded);
    return true;
}

bool DataStore::isExpired(const Entry& entry, Clock::time_point now) {
    return entry.expires_at && *entry.expires_at <= now;
}

void DataStore::removeExpiredLocked(Clock::time_point now) {
    for (auto it = data_.begin(); it != data_.end();) {
        if (isExpired(it->second, now)) {
            it = data_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace redisclone


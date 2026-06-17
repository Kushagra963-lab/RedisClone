#pragma once

#include "command_parser.h"
#include "datastore.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>

namespace redisclone {

struct RuntimeStats {
    std::size_t clients = 0;
    std::chrono::seconds uptime{0};
};

struct CommandResult {
    std::string response;
    bool close_connection = false;
};

class CommandExecutor {
public:
    using StatsProvider = std::function<RuntimeStats()>;

    CommandExecutor(DataStore& store,
                    std::filesystem::path dump_path,
                    StatsProvider stats_provider = {});

    CommandResult execute(const std::string& input);
    CommandResult execute(const Command& command);

private:
    DataStore& store_;
    std::filesystem::path dump_path_;
    StatsProvider stats_provider_;
    std::chrono::steady_clock::time_point started_at_;

    RuntimeStats runtimeStats() const;
    std::string info();
};

} // namespace redisclone

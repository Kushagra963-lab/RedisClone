#include "command_executor.h"

#include <sstream>
#include <stdexcept>

namespace redisclone {

namespace {

std::string joinValue(const std::vector<std::string>& args, std::size_t start) {
    std::ostringstream joined;
    for (std::size_t i = start; i < args.size(); ++i) {
        if (i != start) {
            joined << ' ';
        }
        joined << args[i];
    }
    return joined.str();
}

bool parsePositiveSeconds(const std::string& text, std::chrono::seconds& ttl) {
    try {
        std::size_t consumed = 0;
        const long long value = std::stoll(text, &consumed);
        if (consumed != text.size() || value <= 0) {
            return false;
        }
        ttl = std::chrono::seconds(value);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

CommandExecutor::CommandExecutor(DataStore& store,
                                 std::filesystem::path dump_path,
                                 StatsProvider stats_provider)
    : store_(store),
      dump_path_(std::move(dump_path)),
      stats_provider_(std::move(stats_provider)),
      started_at_(std::chrono::steady_clock::now()) {}

CommandResult CommandExecutor::execute(const std::string& input) {
    return execute(CommandParser::parse(input));
}

CommandResult CommandExecutor::execute(const Command& command) {
    switch (command.type) {
    case CommandType::Empty:
        return {"", false};

    case CommandType::Set:
        if (command.args.size() < 2) {
            return {"ERR usage: SET <key> <value>", false};
        }
        store_.set(command.args[0], joinValue(command.args, 1));
        return {"OK", false};

    case CommandType::Get:
        if (command.args.size() != 1) {
            return {"ERR usage: GET <key>", false};
        }
        if (auto value = store_.get(command.args[0])) {
            return {*value, false};
        }
        return {"(nil)", false};

    case CommandType::Del:
        if (command.args.size() != 1) {
            return {"ERR usage: DEL <key>", false};
        }
        store_.del(command.args[0]);
        return {"OK", false};

    case CommandType::Exists:
        if (command.args.size() != 1) {
            return {"ERR usage: EXISTS <key>", false};
        }
        return {store_.exists(command.args[0]) ? "1" : "0", false};

    case CommandType::Expire: {
        if (command.args.size() != 2) {
            return {"ERR usage: EXPIRE <key> <seconds>", false};
        }
        std::chrono::seconds ttl{0};
        if (!parsePositiveSeconds(command.args[1], ttl)) {
            return {"ERR seconds must be a positive integer", false};
        }
        return {store_.expire(command.args[0], ttl) ? "1" : "0", false};
    }

    case CommandType::Keys: {
        if (!command.args.empty()) {
            return {"ERR usage: KEYS", false};
        }
        const auto keys = store_.keys();
        if (keys.empty()) {
            return {"(empty)", false};
        }
        std::ostringstream out;
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (i != 0) {
                out << '\n';
            }
            out << keys[i];
        }
        return {out.str(), false};
    }

    case CommandType::Save: {
        if (!command.args.empty()) {
            return {"ERR usage: SAVE", false};
        }
        std::string error;
        if (!store_.save(dump_path_, &error)) {
            return {"ERR save failed: " + error, false};
        }
        return {"OK", false};
    }

    case CommandType::Info:
        if (!command.args.empty()) {
            return {"ERR usage: INFO", false};
        }
        return {info(), false};

    case CommandType::Help:
        return {"Commands: SET GET DEL EXISTS EXPIRE KEYS SAVE INFO HELP QUIT", false};

    case CommandType::Quit:
        return {"OK", true};

    case CommandType::Unknown:
        return {"ERR unknown command: " + command.name, false};
    }

    return {"ERR unsupported command", false};
}

RuntimeStats CommandExecutor::runtimeStats() const {
    if (stats_provider_) {
        return stats_provider_();
    }

    const auto elapsed = std::chrono::steady_clock::now() - started_at_;
    return {1, std::chrono::duration_cast<std::chrono::seconds>(elapsed)};
}

std::string CommandExecutor::info() {
    const auto stats = runtimeStats();

    std::ostringstream out;
    out << "Keys: " << store_.keyCount() << '\n'
        << "Clients: " << stats.clients << '\n'
        << "Uptime: " << stats.uptime.count() << " sec\n"
        << "Memory: " << store_.estimateMemoryBytes() << " bytes";
    return out.str();
}

} // namespace redisclone

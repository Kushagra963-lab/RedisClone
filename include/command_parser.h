#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace redisclone {

enum class CommandType {
    Empty,
    Set,
    Get,
    Del,
    Exists,
    Expire,
    Keys,
    Save,
    Info,
    Help,
    Quit,
    Unknown
};

struct Command {
    CommandType type = CommandType::Empty;
    std::vector<std::string> args;
    std::string name;
};

class CommandParser {
public:
    static Command parse(std::string_view line);
    static std::vector<std::string> tokenize(std::string_view line);
    static std::string toUpper(std::string_view text);
};

} // namespace redisclone


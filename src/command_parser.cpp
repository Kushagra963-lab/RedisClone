#include "command_parser.h"

#include <cctype>

namespace redisclone {

namespace {

CommandType commandTypeFor(const std::string& name) {
    if (name == "SET") {
        return CommandType::Set;
    }
    if (name == "GET") {
        return CommandType::Get;
    }
    if (name == "DEL") {
        return CommandType::Del;
    }
    if (name == "EXISTS") {
        return CommandType::Exists;
    }
    if (name == "EXPIRE") {
        return CommandType::Expire;
    }
    if (name == "KEYS") {
        return CommandType::Keys;
    }
    if (name == "SAVE") {
        return CommandType::Save;
    }
    if (name == "INFO") {
        return CommandType::Info;
    }
    if (name == "HELP") {
        return CommandType::Help;
    }
    if (name == "QUIT" || name == "EXIT") {
        return CommandType::Quit;
    }
    return CommandType::Unknown;
}

} // namespace

Command CommandParser::parse(std::string_view line) {
    auto tokens = tokenize(line);
    if (tokens.empty()) {
        return {};
    }

    Command command;
    command.name = toUpper(tokens.front());
    command.type = commandTypeFor(command.name);
    command.args.assign(tokens.begin() + 1, tokens.end());
    return command;
}

std::vector<std::string> CommandParser::tokenize(std::string_view line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;
    bool escaping = false;
    bool token_started = false;

    for (char ch : line) {
        if (escaping) {
            current.push_back(ch);
            escaping = false;
            token_started = true;
            continue;
        }

        if (in_quotes && ch == '\\') {
            escaping = true;
            continue;
        }

        if (ch == '"') {
            in_quotes = !in_quotes;
            token_started = true;
            continue;
        }

        if (!in_quotes && std::isspace(static_cast<unsigned char>(ch))) {
            if (token_started) {
                tokens.push_back(current);
                current.clear();
                token_started = false;
            }
            continue;
        }

        current.push_back(ch);
        token_started = true;
    }

    if (escaping) {
        current.push_back('\\');
        token_started = true;
    }

    if (token_started) {
        tokens.push_back(current);
    }

    return tokens;
}

std::string CommandParser::toUpper(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (char ch : text) {
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    return result;
}

} // namespace redisclone

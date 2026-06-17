#include "command_executor.h"
#include "datastore.h"
#include "server.h"

#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

redisclone::TcpServer* active_server = nullptr;

void handleSignal(int) {
    if (active_server) {
        active_server->stop();
    }
}

void printUsage(const char* program) {
    std::cout << "Usage:\n"
              << "  " << program << " [--host 0.0.0.0] [--port 6379] [--data data/dump.rdb]\n"
              << "  " << program << " --console [--data data/dump.rdb]\n";
}

std::uint16_t parsePort(const std::string& text) {
    const int port = std::stoi(text);
    if (port <= 0 || port > 65535) {
        throw std::invalid_argument("port must be between 1 and 65535");
    }
    return static_cast<std::uint16_t>(port);
}

} // namespace

int main(int argc, char* argv[]) {
    std::string host = "0.0.0.0";
    std::uint16_t port = 6379;
    std::filesystem::path dump_path = "data/dump.rdb";
    bool console_mode = false;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                printUsage(argv[0]);
                return 0;
            }
            if (arg == "--console") {
                console_mode = true;
                continue;
            }
            if (arg == "--host" && i + 1 < argc) {
                host = argv[++i];
                continue;
            }
            if (arg == "--port" && i + 1 < argc) {
                port = parsePort(argv[++i]);
                continue;
            }
            if (arg == "--data" && i + 1 < argc) {
                dump_path = argv[++i];
                continue;
            }
            std::cerr << "Unknown or incomplete argument: " << arg << "\n\n";
            printUsage(argv[0]);
            return 1;
        }

        redisclone::DataStore store;
        std::string load_error;
        if (!store.load(dump_path, &load_error)) {
            std::cerr << "Failed to load dump: " << load_error << '\n';
            return 1;
        }

        if (console_mode) {
            redisclone::CommandExecutor executor(store, dump_path);
            std::string line;

            std::cout << "RedisClone console. Type HELP for commands or QUIT to exit.\n";
            while (std::cout << "> " && std::getline(std::cin, line)) {
                auto result = executor.execute(line);
                if (!result.response.empty()) {
                    std::cout << result.response << '\n';
                }
                if (result.close_connection) {
                    break;
                }
            }
            return 0;
        }

        redisclone::TcpServer server(store, host, port, dump_path);
        active_server = &server;
        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);

        server.run();
        active_server = nullptr;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}


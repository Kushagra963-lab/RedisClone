#pragma once

#include "datastore.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>

namespace redisclone {

class TcpServer {
public:
    TcpServer(DataStore& store,
              std::string host,
              std::uint16_t port,
              std::filesystem::path dump_path);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    void run();
    void stop();
    std::size_t clientCount() const;
    std::chrono::seconds uptime() const;

private:
    DataStore& store_;
    std::string host_;
    std::uint16_t port_;
    std::filesystem::path dump_path_;
    std::atomic<bool> running_{false};
    std::atomic<std::size_t> client_count_{0};
    std::thread cleaner_;
    int server_fd_ = -1;
    std::chrono::steady_clock::time_point started_at_;

    void startCleaner();
    void cleanerLoop();
    void handleClient(int client_fd);
    void closeServerSocket();
};

} // namespace redisclone


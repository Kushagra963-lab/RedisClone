#include "server.h"

#include "command_executor.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace redisclone {

namespace {

void sendAll(int fd, const std::string& text) {
    const char* data = text.data();
    std::size_t remaining = text.size();

    while (remaining > 0) {
        const ssize_t sent = ::send(fd, data, remaining, 0);
        if (sent <= 0) {
            return;
        }
        data += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
}

std::string withNetworkNewline(std::string response) {
    if (response.empty()) {
        return "\r\n";
    }

    std::string converted;
    converted.reserve(response.size() + 2);
    for (char ch : response) {
        if (ch == '\n') {
            converted += "\r\n";
        } else {
            converted.push_back(ch);
        }
    }
    converted += "\r\n";
    return converted;
}

} // namespace

TcpServer::TcpServer(DataStore& store,
                     std::string host,
                     std::uint16_t port,
                     std::filesystem::path dump_path)
    : store_(store),
      host_(std::move(host)),
      port_(port),
      dump_path_(std::move(dump_path)),
      started_at_(std::chrono::steady_clock::now()) {}

TcpServer::~TcpServer() {
    stop();
    if (cleaner_.joinable()) {
        cleaner_.join();
    }
}

void TcpServer::run() {
    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("socket failed: " + std::string(std::strerror(errno)));
    }

    int reuse = 1;
    if (::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        closeServerSocket();
        throw std::runtime_error("setsockopt failed: " + std::string(std::strerror(errno)));
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);

    if (host_ == "0.0.0.0") {
        address.sin_addr.s_addr = INADDR_ANY;
    } else if (::inet_pton(AF_INET, host_.c_str(), &address.sin_addr) != 1) {
        closeServerSocket();
        throw std::runtime_error("invalid IPv4 host: " + host_);
    }

    if (::bind(server_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        closeServerSocket();
        throw std::runtime_error("bind failed: " + std::string(std::strerror(errno)));
    }

    if (::listen(server_fd_, SOMAXCONN) < 0) {
        closeServerSocket();
        throw std::runtime_error("listen failed: " + std::string(std::strerror(errno)));
    }

    running_ = true;
    started_at_ = std::chrono::steady_clock::now();
    startCleaner();

    std::cout << "RedisClone listening on " << host_ << ':' << port_ << '\n';

    while (running_) {
        sockaddr_in client_address{};
        socklen_t client_length = sizeof(client_address);
        const int client_fd = ::accept(
            server_fd_, reinterpret_cast<sockaddr*>(&client_address), &client_length);

        if (client_fd < 0) {
            if (running_) {
                std::cerr << "accept failed: " << std::strerror(errno) << '\n';
            }
            continue;
        }

        std::thread(&TcpServer::handleClient, this, client_fd).detach();
    }

    closeServerSocket();
}

void TcpServer::stop() {
    running_ = false;
    closeServerSocket();
}

std::size_t TcpServer::clientCount() const {
    return client_count_.load();
}

std::chrono::seconds TcpServer::uptime() const {
    const auto elapsed = std::chrono::steady_clock::now() - started_at_;
    return std::chrono::duration_cast<std::chrono::seconds>(elapsed);
}

void TcpServer::startCleaner() {
    if (!cleaner_.joinable()) {
        cleaner_ = std::thread(&TcpServer::cleanerLoop, this);
    }
}

void TcpServer::cleanerLoop() {
    while (running_) {
        store_.removeExpiredKeys();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void TcpServer::handleClient(int client_fd) {
    client_count_.fetch_add(1);

    CommandExecutor executor(
        store_, dump_path_, [this] { return RuntimeStats{clientCount(), uptime()}; });

    sendAll(client_fd, "RedisClone ready. Type HELP for commands.\r\n");

    std::string pending;
    char buffer[4096];

    while (running_) {
        const ssize_t received = ::recv(client_fd, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            break;
        }

        pending.append(buffer, static_cast<std::size_t>(received));

        std::size_t newline = std::string::npos;
        while ((newline = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            auto result = executor.execute(line);
            if (!result.response.empty()) {
                sendAll(client_fd, withNetworkNewline(result.response));
            }
            if (result.close_connection) {
                ::close(client_fd);
                client_count_.fetch_sub(1);
                return;
            }
        }
    }

    ::close(client_fd);
    client_count_.fetch_sub(1);
}

void TcpServer::closeServerSocket() {
    if (server_fd_ >= 0) {
        ::shutdown(server_fd_, SHUT_RDWR);
        ::close(server_fd_);
        server_fd_ = -1;
    }
}

} // namespace redisclone


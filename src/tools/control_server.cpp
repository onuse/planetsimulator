#include "tools/control_server.hpp"

#include <cstring>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
static constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
#define CLOSE_SOCKET closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static constexpr socket_t INVALID_SOCK = -1;
#define CLOSE_SOCKET ::close
#endif

namespace tools {

namespace {

// Winsock has to be started once per process and stopped once. Doing it in a
// static keeps the lifetime tied to the program rather than to any one server.
struct SocketSubsystem {
    bool ok = false;

    SocketSubsystem() {
#ifdef _WIN32
        WSADATA data;
        ok = WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
        ok = true;
#endif
    }

    ~SocketSubsystem() {
#ifdef _WIN32
        if (ok) {
            WSACleanup();
        }
#endif
    }
};

SocketSubsystem& sockets() {
    static SocketSubsystem instance;
    return instance;
}

} // namespace

ControlServer::~ControlServer() {
    stop();
}

bool ControlServer::start(unsigned short requestedPort) {
    if (running.load()) {
        return true;
    }
    if (!sockets().ok) {
        std::cerr << "Control: sockets unavailable\n";
        return false;
    }

    const socket_t listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listener_ == INVALID_SOCK) {
        std::cerr << "Control: could not create a socket\n";
        return false;
    }

    // Otherwise a restart inside the operating system's close-wait window is
    // refused the port it just had, which in practice means every second run.
    int reuse = 1;
    ::setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(requestedPort);
    // Loopback only. This accepts unauthenticated commands that move the
    // camera and drive the simulation, so it has no business being reachable
    // from anywhere but this machine.
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(listener_, 1) != 0) {
        std::cerr << "Control: port " << requestedPort << " is not available\n";
        CLOSE_SOCKET(listener_);
        return false;
    }

    listenSocket.store(static_cast<long long>(listener_));
    port = requestedPort;
    running.store(true);
    listener = std::thread([this]() { acceptLoop(); });

    std::cout << "Control: listening on 127.0.0.1:" << port << std::endl;
    return true;
}

void ControlServer::stop() {
    if (!running.exchange(false)) {
        return;
    }

    // Closing the sockets is what wakes the listener thread out of accept or
    // recv; there is no portable way to interrupt either of them otherwise.
    const long long client = clientSocket.exchange(-1);
    if (client >= 0) {
        CLOSE_SOCKET(static_cast<socket_t>(client));
    }
    const long long listening = listenSocket.exchange(-1);
    if (listening >= 0) {
        CLOSE_SOCKET(static_cast<socket_t>(listening));
    }

    if (listener.joinable()) {
        listener.join();
    }
}

void ControlServer::acceptLoop() {
    std::string buffer;

    while (running.load()) {
        const long long listening = listenSocket.load();
        if (listening < 0) {
            break;
        }

        const socket_t connection = ::accept(static_cast<socket_t>(listening), nullptr, nullptr);
        if (connection == INVALID_SOCK) {
            if (!running.load()) {
                break;
            }
            continue;
        }

        clientSocket.store(static_cast<long long>(connection));
        buffer.clear();

        // One client at a time, and it holds the connection for the length of
        // a session. Commands are strictly ordered, which is the whole point:
        // "advance two million years" then "measure" is a sequence, and a
        // second client interleaving with it would make both answers useless.
        while (running.load()) {
            char chunk[1024];
            const int got = ::recv(connection, chunk, sizeof(chunk), 0);
            if (got <= 0) {
                break;
            }
            buffer.append(chunk, static_cast<size_t>(got));

            size_t newline;
            while ((newline = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, newline);
                buffer.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (line.empty()) {
                    continue;
                }
                std::lock_guard<std::mutex> lock(queueMutex);
                pending.push_back(std::move(line));
            }
        }

        const long long current = clientSocket.exchange(-1);
        if (current >= 0) {
            CLOSE_SOCKET(static_cast<socket_t>(current));
        }
    }
}

bool ControlServer::poll(std::string& command) {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (pending.empty()) {
        return false;
    }
    command = std::move(pending.front());
    pending.pop_front();
    return true;
}

void ControlServer::reply(const std::string& text) {
    const long long client = clientSocket.load();
    if (client < 0) {
        return;
    }

    std::string payload = text;
    if (payload.empty() || payload.back() != '\n') {
        payload.push_back('\n');
    }

    size_t sent = 0;
    while (sent < payload.size()) {
        const int wrote = ::send(static_cast<socket_t>(client), payload.data() + sent,
                                 static_cast<int>(payload.size() - sent), 0);
        if (wrote <= 0) {
            return;
        }
        sent += static_cast<size_t>(wrote);
    }
}

} // namespace tools

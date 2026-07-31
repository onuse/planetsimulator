#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace tools {

// A command channel into the running simulator.
//
// The reason this exists is that most of what goes wrong here only goes wrong
// over time. A drainage network that reorganises sensibly for two million years
// and then erases itself looks perfectly healthy in any single frame, and the
// only way to catch it is to drive the thing: put the camera somewhere
// specific, advance a known amount of geological time, and read the same
// quantity again. None of that is possible from command line flags decided
// before the process starts.
//
// Line in, line out, over a socket on the loopback interface. Deliberately not
// a binary protocol and deliberately not a framework: it has to be usable by
// hand from a terminal, because the first thing anyone does when a tool like
// this misbehaves is try it manually.
//
// The socket only ever accepts connections from this machine.
class ControlServer {
public:
    ~ControlServer();

    // Start listening. Returns false if the port is taken or sockets are
    // unavailable, in which case the application carries on without a control
    // channel rather than refusing to run.
    bool start(unsigned short port);
    void stop();

    bool isRunning() const { return running.load(); }
    unsigned short getPort() const { return port; }

    // Take the next command, if one has arrived. Called from the main thread
    // between frames, so command handlers run where the camera, the renderer
    // and the planet are all safe to touch and no locking is needed for any of
    // them.
    bool poll(std::string& command);

    // Answer the command currently being handled. Exactly one reply per
    // command, or the client and the server stop agreeing about which answer
    // belongs to which question.
    void reply(const std::string& text);

private:
    void acceptLoop();

    std::thread listener;
    std::atomic<bool> running{false};
    unsigned short port = 0;

    // Kept as a plain integer rather than SOCKET so that no header outside the
    // implementation has to know what platform this is.
    std::atomic<long long> listenSocket{-1};
    std::atomic<long long> clientSocket{-1};

    std::mutex queueMutex;
    std::deque<std::string> pending;
};

} // namespace tools

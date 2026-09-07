// Just the one function, but it's the most important one in the project -
// see the comment inside for why the lock and the connected check can't be
// reordered.
#include "network/session.hpp"

void send_line(Session& session, const std::string& message) {
    // The `connected` check MUST live inside the lock, not before it: it's
    // the same write_mutex that client_loop takes to set connected=false and
    // close the socket (see server.cpp). This makes send and close mutually
    // exclusive, so we can never send() on an fd that's already been closed
    // (and potentially reassigned by a concurrent accept()).
    std::lock_guard<std::mutex> lock(session.write_mutex);
    if (!session.connected) return;
    std::string line = message + "\n";
    net::send_all(session.socket_fd, line.data(), line.size());
}

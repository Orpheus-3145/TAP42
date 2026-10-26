// Just the one function, but it's the most important one in the project -
// see the comment inside for why the lock and the connected check can't be
// reordered.
#include "network/session.hpp"

#include <sstream>

#include "logging/logger.hpp"

namespace {
// Logs what was sent, keyed on the first tokens of the wire line (OK / ERR <code>
// <NAME> / EVT <scope> <type>). The payload is deliberately not logged: it can be
// large (LOOK) and the command log already says what was asked.
void log_outgoing(const Session& session, const std::string& message) {
    std::istringstream in(message);
    std::string kind, a, b;
    in >> kind >> a >> b;
    const std::string player = session.player_id.empty() ? "-" : session.player_id;
    if (kind == "ERR")
        log_warn("error_sent", {{"player", player}, {"code", a}, {"name", b}});
    else if (kind == "EVT")
        log_info("event_sent", {{"player", player}, {"evt", a + " " + b}});
    else
        log_info("response_sent", {{"player", player}, {"status", kind}});
}
}  // namespace

void send_line(Session& session, const std::string& message) {
    // The `connected` check MUST live inside the lock, not before it: it's
    // the same write_mutex that client_loop takes to set connected=false and
    // close the socket (see server.cpp). This makes send and close mutually
    // exclusive, so we can never send() on an fd that's already been closed
    // (and potentially reassigned by a concurrent accept()).
    {
        std::lock_guard<std::mutex> lock(session.write_mutex);
        if (!session.connected) return;
        std::string line = message + "\n";
        net::send_all(session.socket_fd, line.data(), line.size());
    }
    // Logged after the lock is released so logging never delays other writers.
    log_outgoing(session, message);
}

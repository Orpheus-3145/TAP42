// The MAX_LINE_LENGTH cap below exists because a client that never sends a
// newline would otherwise grow recv_buffer forever - easiest DoS in the book.
#include "network/line_reader.hpp"

#include "logging/logger.hpp"
#include "network/platform_socket.hpp"

namespace {
// A client that sends data without ever including a '\n' would make
// recv_buffer grow without bound (a memory DoS). Past this threshold we
// treat the connection as abusive and close it instead of keep accumulating.
constexpr size_t MAX_LINE_LENGTH = 8192;

// Only applies once a message has started arriving but hasn't been
// terminated yet: a player who's simply idle between commands (empty
// recv_buffer) still gets a normal indefinite block, no timeout involved.
constexpr int PARTIAL_MESSAGE_TIMEOUT_MS = 5000;
} // namespace

bool read_line(Session& session, std::string& out_line) {
    size_t newline_pos;
    while ((newline_pos = session.recv_buffer.find('\n')) == std::string::npos) {
        if (session.recv_buffer.size() > MAX_LINE_LENGTH) {
            log_warn("line_too_long", {{"ip", session.peer_ip}, {"size", std::to_string(session.recv_buffer.size())}});
            return false;
        }
        net::set_recv_timeout(session.socket_fd,
                               session.recv_buffer.empty() ? 0 : PARTIAL_MESSAGE_TIMEOUT_MS);
        char buf[4096];
        int n = net::recv_some(session.socket_fd, buf, sizeof(buf));
        if (n <= 0) {
            if (!session.recv_buffer.empty()) {
                log_warn("partial_message_stalled",
                         {{"ip", session.peer_ip}, {"buffered", std::to_string(session.recv_buffer.size())}});
            }
            return false; // connection closed, errored, or timed out mid-message
        }
        session.recv_buffer.append(buf, static_cast<size_t>(n));
    }
    out_line = session.recv_buffer.substr(0, newline_pos);
    if (!out_line.empty() && out_line.back() == '\r') out_line.pop_back();
    session.recv_buffer.erase(0, newline_pos + 1);
    return true;
}

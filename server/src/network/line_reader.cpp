#include "network/line_reader.hpp"

#include "logging/logger.hpp"
#include "network/platform_socket.hpp"

namespace {
// A client that sends data without ever including a '\n' would make
// recv_buffer grow without bound (a memory DoS). Past this threshold we
// treat the connection as abusive and close it instead of keep accumulating.
constexpr size_t MAX_LINE_LENGTH = 8192;
} // namespace

bool read_line(Session& session, std::string& out_line) {
    size_t newline_pos;
    while ((newline_pos = session.recv_buffer.find('\n')) == std::string::npos) {
        if (session.recv_buffer.size() > MAX_LINE_LENGTH) {
            log_warn("line_too_long", {{"ip", session.peer_ip}, {"size", std::to_string(session.recv_buffer.size())}});
            return false;
        }
        char buf[4096];
        int n = net::recv_some(session.socket_fd, buf, sizeof(buf));
        if (n <= 0) return false; // connection closed or errored
        session.recv_buffer.append(buf, static_cast<size_t>(n));
    }
    out_line = session.recv_buffer.substr(0, newline_pos);
    if (!out_line.empty() && out_line.back() == '\r') out_line.pop_back();
    session.recv_buffer.erase(0, newline_pos + 1);
    return true;
}

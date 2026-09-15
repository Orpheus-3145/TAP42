// Nothing exciting here except platform_init() ignoring SIGPIPE on POSIX -
// without that, a send() to a client who just vanished takes the whole
// server down with it.
#include "network/platform_socket.hpp"

#ifndef _WIN32
  #include <cerrno>
  #include <csignal>
#endif

namespace net {

bool platform_init() {
#ifdef _WIN32
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0;
#else
    // Without this, a send() on a socket whose peer has already closed the
    // connection (e.g. a broadcast to a client that just disconnected)
    // raises SIGPIPE, which by default terminates the whole server process.
    // The code already handles send() errors another way (send_all ignores
    // the return value by design: it's best-effort, the owning thread's next
    // recv() will detect the disconnect), so we just need to stop the signal
    // from killing the process.
    std::signal(SIGPIPE, SIG_IGN);
    return true;
#endif
}

void platform_cleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

void close_socket(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

void set_recv_timeout(socket_t s, int timeout_ms) {
#ifdef _WIN32
    DWORD timeout = static_cast<DWORD>(timeout_ms);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

int send_all(socket_t s, const char* data, size_t len) {
    // A single send() is NOT guaranteed to write the whole buffer, even on a
    // blocking socket - it can do a short write (e.g. interrupted by a
    // signal). Loop until every byte is actually sent, or a real error hits.
    size_t sent = 0;
    while (sent < len) {
#ifdef _WIN32
        int n = send(s, data + sent, static_cast<int>(len - sent), 0);
        if (n <= 0) return n;
#else
        ssize_t n = send(s, data + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue; // interrupted before writing anything, just retry
            return static_cast<int>(n);
        }
        if (n == 0) return 0;
#endif
        sent += static_cast<size_t>(n);
    }
    return static_cast<int>(sent);
}

int recv_some(socket_t s, char* buf, size_t len) {
#ifdef _WIN32
    return recv(s, buf, static_cast<int>(len), 0);
#else
    return static_cast<int>(recv(s, buf, len, 0));
#endif
}

} // namespace net

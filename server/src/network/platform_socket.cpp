#include "network/platform_socket.hpp"

#ifndef _WIN32
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

int send_all(socket_t s, const char* data, size_t len) {
#ifdef _WIN32
    return send(s, data, static_cast<int>(len), 0);
#else
    return static_cast<int>(send(s, data, len, 0));
#endif
}

int recv_some(socket_t s, char* buf, size_t len) {
#ifdef _WIN32
    return recv(s, buf, static_cast<int>(len), 0);
#else
    return static_cast<int>(recv(s, buf, len, 0));
#endif
}

} // namespace net

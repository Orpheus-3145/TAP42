// Thin wrapper around raw sockets so the rest of the codebase doesn't have
// to sprinkle #ifdefs everywhere for Windows vs POSIX.
#pragma once

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
using socket_t_impl = SOCKET;
  #define INVALID_SOCK_VALUE INVALID_SOCKET
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
using socket_t_impl = int;
  #define INVALID_SOCK_VALUE (-1)
#endif

namespace net {

using socket_t = socket_t_impl;
constexpr socket_t INVALID_SOCK = INVALID_SOCK_VALUE;

// Initializes Winsock on Windows (no-op elsewhere). Call once at startup.
bool platform_init();

// Releases Winsock resources on Windows (no-op elsewhere).
void platform_cleanup();

void close_socket(socket_t s);

int send_all(socket_t s, const char* data, size_t len);
int recv_some(socket_t s, char* buf, size_t len);

} // namespace net

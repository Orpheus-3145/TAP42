#include "network/server.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "commands/dispatcher.hpp"
#include "logging/abuse_monitor.hpp"
#include "logging/logger.hpp"
#include "network/line_reader.hpp"
#include "network/platform_socket.hpp"
#include "network/session.hpp"

namespace {

void client_loop(net::socket_t client_fd, std::string peer_ip) {
    auto session = std::make_shared<Session>();
    session->socket_fd = client_fd;
    session->peer_ip = peer_ip;

    log_info("client_connected", {{"ip", peer_ip}});

    std::string line;
    while (session->connected && read_line(*session, line)) {
        handle_command(session, line);
    }

    handle_disconnect(session);

    // connected=false and close() under the same write_mutex as send_line():
    // guarantees no other thread can send() on this fd after it has been
    // closed (and possibly reassigned by a concurrent accept() on another
    // client).
    {
        std::lock_guard<std::mutex> lock(session->write_mutex);
        session->connected = false;
        net::close_socket(client_fd);
    }
    log_info("client_disconnected", {{"ip", peer_ip}});
}

} // namespace

int run_server(uint16_t port) {
    if (!net::platform_init()) {
        log_error("platform_init_failed", {});
        return 1;
    }

    net::socket_t listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == net::INVALID_SOCK) {
        log_error("socket_create_failed", {});
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        log_error("bind_failed", {{"port", std::to_string(port)}});
        return 1;
    }

    if (listen(listen_fd, 16) != 0) {
        log_error("listen_failed", {});
        return 1;
    }

    log_info("server_started", {{"port", std::to_string(port)}});

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        net::socket_t client_fd =
            accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd == net::INVALID_SOCK) {
            log_warn("accept_failed", {});
            continue;
        }
        char ip_buf[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf));
        std::string ip(ip_buf);
        if (record_connection_and_check_rapid_reconnect(ip)) {
            log_warn("rapid_reconnect_detected", {{"ip", ip}});
        }
        std::thread(client_loop, client_fd, ip).detach();
    }
}

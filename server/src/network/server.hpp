#pragma once

#include <cstdint>

// Bind + listen + accept loop. Spawns one thread per accepted client.
// Blocking: only returns on a fatal bind/listen error.
int run_server(uint16_t port);

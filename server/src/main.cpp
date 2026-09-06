#include <cstdlib>
#include <iostream>

#include "network/server.hpp"
#include "world/loader.hpp"

int main(int argc, char** argv) {
    uint16_t port = 4242;
    if (argc > 1) port = static_cast<uint16_t>(std::atoi(argv[1]));

    if (!load_world("data/world.json")) {
        std::cerr << "Failed to load world data\n";
        return 1;
    }

    return run_server(port);
}

// Singleton on purpose - there's exactly one world per server process.
#include "world/world.hpp"

World& World::instance() {
    static World w;
    return w;
}

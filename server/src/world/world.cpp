#include "world/world.hpp"

World& World::instance() {
    static World w;
    return w;
}

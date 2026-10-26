CLIENT_DIR        := client
CLIENT_BUILD_DIR  := $(CLIENT_DIR)/build
SERVER_DIR        := server
# Use the same C++ compiler as the server (g++ by default) instead of whatever `c++` points to.
CMAKE_ARGS        ?= -DCMAKE_CXX_COMPILER=$(CXX)

.PHONY: all configure cli gui server run-cli run-gui run-server run-client run-client-gui deps lint clean fclean

all: cli

configure:
	cmake -S $(CLIENT_DIR) -B $(CLIENT_BUILD_DIR) $(CMAKE_ARGS)

configure-debug:
	cmake -S $(CLIENT_DIR) -B $(CLIENT_BUILD_DIR) $(CMAKE_ARGS) -DENABLE_SANITIZERS=ON

cli: configure
	cmake --build $(CLIENT_BUILD_DIR) --target tap-client-cli -j

gui: configure
	cmake --build $(CLIENT_BUILD_DIR) --target tap-client-gui -j

server:
	$(MAKE) -C $(SERVER_DIR) all

run-cli: configure
	cmake --build $(CLIENT_BUILD_DIR) --target run-cli

run-gui: configure
	cmake --build $(CLIENT_BUILD_DIR) --target run-gui

run-server:
	$(MAKE) -C $(SERVER_DIR) run

run-cli-debug: configure-debug
	cmake --build $(CLIENT_BUILD_DIR) --target run-cli

run-gui-debug: configure-debug
	cmake --build $(CLIENT_BUILD_DIR) --target run-gui

clean:
	-cmake --build $(CLIENT_BUILD_DIR) --target clean
	$(MAKE) -C $(SERVER_DIR) clean

fclean:
	rm -rf $(CLIENT_BUILD_DIR)
	$(MAKE) -C $(SERVER_DIR) clean

# Names required by the subject (aliases of the targets above)
run-client: run-cli

run-client-gui: run-gui

deps:
	$(MAKE) -C $(SERVER_DIR) deps
	@echo "Client needs: cmake, a C++20 compiler, libncurses-dev (CLI) and qt6-base-dev (GUI)."
	@command -v cmake >/dev/null 2>&1 || echo "MISSING: cmake"
	@pkg-config --exists ncurses 2>/dev/null || echo "MISSING: libncurses-dev"

lint:
	$(MAKE) -C $(SERVER_DIR) lint

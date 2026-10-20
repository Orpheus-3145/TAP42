CLIENT_DIR        := client
CLIENT_BUILD_DIR  := $(CLIENT_DIR)/build
SERVER_DIR        := server
CMAKE_ARGS        ?=

.PHONY: all configure cli gui server run-cli run-gui run-server clean fclean

all: cli

configure:
	cmake -S $(CLIENT_DIR) -B $(CLIENT_BUILD_DIR) $(CMAKE_ARGS)

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

clean:
	-cmake --build $(CLIENT_BUILD_DIR) --target clean
	$(MAKE) -C $(SERVER_DIR) clean

fclean:
	rm -rf $(CLIENT_BUILD_DIR)
	$(MAKE) -C $(SERVER_DIR) clean

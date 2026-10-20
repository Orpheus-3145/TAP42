BUILD_DIR  := build
CMAKE_ARGS ?=

.PHONY: all configure cli gui run-cli run-gui clean fclean re-cli re-gui

all: cli

configure:
	cmake -B $(BUILD_DIR) $(CMAKE_ARGS)

cli: configure
	cmake --build $(BUILD_DIR) --target tap-client-cli -j

gui: configure
	cmake --build $(BUILD_DIR) --target tap-client-gui -j

run-cli: configure
	cmake --build $(BUILD_DIR) --target run-cli

run-gui: configure
	cmake --build $(BUILD_DIR) --target run-gui

clean:
	-cmake --build $(BUILD_DIR) --target clean

fclean:
	rm -rf $(BUILD_DIR)

re-cli: fclean cli

re-gui: fclean gui

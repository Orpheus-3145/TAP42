CC              := c++
DEFAULT_CONFIG  := -i 127.0.0.1 -p 4242

CPP_FLAGS       := -std=c++20 -Wall -Wextra -Werror
DEBUG_FLAGS     := -O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
DEPS_FLAGS      := -MMD -MP -MF

CLIENT_DIR      := client
CLI_DIR         := $(CLIENT_DIR)/cli
GUI_DIR         := $(CLIENT_DIR)/gui
SHARED_DIR      := $(CLIENT_DIR)/shared
LOG_DIR         := $(CLIENT_DIR)/logs

CLI_SRC_DIR     := $(CLI_DIR)/source
GUI_SRC_DIR     := $(GUI_DIR)/source
SHARED_SRC_DIR  := $(SHARED_DIR)/source

BUILD_DIR       := $(CLIENT_DIR)/build
DEPS_DIR         = $(BUILD_DIR)/deps
OBJ_DIR          = $(BUILD_DIR)/obj

CLI_SOURCES      = $(shell find $(CLI_SRC_DIR) -type f -name '*.cpp')
GUI_SOURCES      = $(shell find $(GUI_SRC_DIR) -type f -name '*.cpp')
SHARED_SOURCES   = $(shell find $(SHARED_SRC_DIR) -type f -name '*.cpp')

CLI_OBJECTS      = $(patsubst $(CLI_SRC_DIR)/%.cpp,$(OBJ_DIR)/client/%.o,$(CLI_SOURCES))
gui_OBJECTS      = $(patsubst $(GUI_SRC_DIR)/%.cpp,$(OBJ_DIR)/client/%.o,$(GUI_SOURCES))
SHARED_OBJECTS   = $(patsubst $(SHARED_SRC_DIR)/%.cpp,$(OBJ_DIR)/shared/%.o,$(SHARED_SOURCES))

OBJECTS          = $(CLI_SOURCES) $(GUI_SOURCES) $(SHARED_OBJECTS)

CLI_DEPS         = $(patsubst $(CLI_SRC_DIR)/%.cpp,$(DEPS_DIR)/client/%.d,$(CLI_SOURCES))
GUI_DEPS         = $(patsubst $(GUI_SRC_DIR)/%.cpp,$(DEPS_DIR)/client/%.d,$(GUI_SOURCES))
SHARED_DEPS      = $(patsubst $(SHARED_SRC_DIR)/%.cpp,$(DEPS_DIR)/shared/%.d,$(SHARED_SOURCES))

DEPS             = $(CLI_DEPS) $(GUI_DEPS) $(SHARED_DEPS)

INCLUDE         := -I$(CLI_DIR)/include -I$(GUI_DIR)/include -I$(SHARED_DIR)/include
LIBS            := -lncurses
TARGET          := $(CLIENT_DIR)/client


all: $(LOG_DIR) $(TARGET)

run: all
	./$(TARGET) $(DEFAULT_CONFIG)

$(LOG_DIR):
	mkdir -p $(LOG_DIR)

$(TARGET): $(OBJECTS)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(OBJECTS) $(LIBS) -o $@

$(OBJ_DIR)/client/cli/%.o: $(CLI_SRC_DIR)/%.cpp
	@mkdir -p $(dir $@) $(dir $(DEPS_DIR)/client/cli/$*.d)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(DEPS_FLAGS) $(DEPS_DIR)/client/cli/$*.d -c $< -o $@

$(OBJ_DIR)/client/gui/%.o: $(GUI_SRC_DIR)/%.cpp
	@mkdir -p $(dir $@) $(dir $(DEPS_DIR)/client/gui/$*.d)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(DEPS_FLAGS) $(DEPS_DIR)/client/gui/$*.d -c $< -o $@

$(OBJ_DIR)/shared/%.o: $(SHARED_SRC_DIR)/%.cpp
	@mkdir -p $(dir $@) $(dir $(DEPS_DIR)/shared/$*.d)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(DEPS_FLAGS) $(DEPS_DIR)/shared/$*.d -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

fclean: clean

re: fclean all

.PHONY: all run clean fclean re
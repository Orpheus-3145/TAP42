TARGET_CLIENT   := client
CC              := c++
DEFAULT_CONFIG  := -i 127.0.0.1 -p 4242

CPP_FLAGS       := -std=c++17 -Wall -Wextra -Werror
DEBUG_FLAGS     := -O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
DEPS_FLAGS      := -MMD -MP -MF

CLIENT_DIR      := clients
SHARED_DIR      := shared
LOG_DIR         := logs

CLIENT_SRC_DIR  := $(CLIENT_DIR)/source
SHARED_SRC_DIR  := $(SHARED_DIR)/source

BUILD_DIR       := $(CLIENT_DIR)/build
DEPS_DIR         = $(BUILD_DIR)/deps
OBJ_DIR          = $(BUILD_DIR)/obj

CLIENT_SOURCES   = $(shell find $(CLIENT_SRC_DIR) -type f -name '*.cpp')
SHARED_SOURCES   = $(shell find $(SHARED_SRC_DIR) -type f -name '*.cpp')

CLIENT_OBJECTS   = $(patsubst $(CLIENT_SRC_DIR)/%.cpp,$(OBJ_DIR)/client/%.o,$(CLIENT_SOURCES))
SHARED_OBJECTS   = $(patsubst $(SHARED_SRC_DIR)/%.cpp,$(OBJ_DIR)/shared/%.o,$(SHARED_SOURCES))

OBJECTS          = $(CLIENT_OBJECTS) $(SHARED_OBJECTS)

CLIENT_DEPS      = $(patsubst $(CLIENT_SRC_DIR)/%.cpp,$(DEPS_DIR)/client/%.d,$(CLIENT_SOURCES))
SHARED_DEPS      = $(patsubst $(SHARED_SRC_DIR)/%.cpp,$(DEPS_DIR)/shared/%.d,$(SHARED_SOURCES))

DEPS             = $(CLIENT_DEPS) $(SHARED_DEPS)

INCLUDE         := -I$(CLIENT_DIR)/include -Ishared/include
LIBS            := -lncurses


all: $(LOG_DIR) $(TARGET_CLIENT)

run: all
	./$(TARGET_CLIENT) $(DEFAULT_CONFIG)

$(LOG_DIR):
	mkdir -p $(LOG_DIR)

$(TARGET_CLIENT): $(OBJECTS)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(OBJECTS) $(LIBS) -o $@

$(OBJ_DIR)/client/%.o: $(CLIENT_SRC_DIR)/%.cpp
	mkdir -p $(dir $@) $(dir $(DEPS_DIR)/client/$*.d)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(DEPS_FLAGS) $(DEPS_DIR)/client/$*.d -c $< -o $@

$(OBJ_DIR)/shared/%.o: $(SHARED_SRC_DIR)/%.cpp
	mkdir -p $(dir $@) $(dir $(DEPS_DIR)/shared/$*.d)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(DEPS_FLAGS) $(DEPS_DIR)/shared/$*.d -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

fclean: clean

re: fclean all

.PHONY: all run clean fclean re
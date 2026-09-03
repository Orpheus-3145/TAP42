TARGET_CLIENT   := client
CC              := c++
DEFAULT_CONFIG  := -i 127.0.0.1 -p 4242

CPP_FLAGS       := -std=c++17 -Wall -Wextra -Werror
DEBUG_FLAGS     := -O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
DEPS_FLAGS      := -MMD -MP -MF

CLIENT_DIR		:= clients
SRC_DIR         := $(CLIENT_DIR)/source
BUILD_DIR       := $(CLIENT_DIR)/build
DEPS_DIR         = $(BUILD_DIR)/deps
OBJ_DIR          = $(BUILD_DIR)/obj

SOURCES          = $(shell find $(SRC_DIR) -type f -name '*.cpp')
OBJECTS          = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS             = $(patsubst $(SRC_DIR)/%.cpp,$(DEPS_DIR)/%.d,$(SOURCES))

INCLUDE         := -I$(CLIENT_DIR)/include -Ishared/include
LIBS            := -lreadline -lncurses


all: $(TARGET_CLIENT)

run: all
	./$(TARGET_CLIENT) $(DEFAULT_CONFIG) 

$(OBJ_DIR) $(DEPS_DIR) $(SHADERS_OUT_DIR):
	mkdir -p $@

$(TARGET_CLIENT): $(OBJ_DIR) $(DEPS_DIR) $(OBJECTS)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(LIBS) $(OBJECTS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(DEPS_FLAGS) $(DEPS_DIR)/$*.d -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

fclean: clean

re: fclean all

.PHONY: all run clean fclean re

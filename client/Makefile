CC              := clang++
TARGET          := tap-client
CLI_LIB         := libTapCLI.a
GUI_LIB         := libTapGUI.a
DEFAULT_CONFIG  := -i 127.0.0.1 -p 4242

CPP_FLAGS       := -std=c++20 -Wall -Wextra -Werror
DEBUG_FLAGS     := -O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
DEPS_FLAGS      := -MMD -MP -MF

CLI_DIR         := cli
GUI_DIR         := gui
SHARED_DIR      := shared
LOG_DIR         := logs

CLI_SRC_DIR     := $(CLI_DIR)/source
GUI_SRC_DIR     := $(GUI_DIR)/source
SHARED_SRC_DIR  := $(SHARED_DIR)/source

BUILD_DIR       := build
DEPS_DIR         = $(BUILD_DIR)/deps
OBJ_DIR          = $(BUILD_DIR)/obj

CLI_SOURCES      = $(shell find $(CLI_SRC_DIR) -type f -name '*.cpp')
GUI_SOURCES      = $(shell find $(GUI_SRC_DIR) -type f -name '*.cpp')
SHARED_SOURCES   = $(shell find $(SHARED_SRC_DIR) -type f -name '*.cpp')

CLI_OBJECTS      = $(patsubst $(CLI_SRC_DIR)/%.cpp,$(OBJ_DIR)/client/cli/%.o,$(CLI_SOURCES))
GUI_OBJECTS      = $(patsubst $(GUI_SRC_DIR)/%.cpp,$(OBJ_DIR)/client/gui/%.o,$(GUI_SOURCES))
SHARED_OBJECTS   = $(patsubst $(SHARED_SRC_DIR)/%.cpp,$(OBJ_DIR)/shared/%.o,$(SHARED_SOURCES))

OBJECTS          = $(CLI_OBJECTS) $(GUI_OBJECTS) $(SHARED_OBJECTS)

CLI_DEPS         = $(patsubst $(CLI_SRC_DIR)/%.cpp,$(DEPS_DIR)/client/cli/%.d,$(CLI_SOURCES))
GUI_DEPS         = $(patsubst $(GUI_SRC_DIR)/%.cpp,$(DEPS_DIR)/client/gui/%.d,$(GUI_SOURCES))
SHARED_DEPS      = $(patsubst $(SHARED_SRC_DIR)/%.cpp,$(DEPS_DIR)/shared/%.d,$(SHARED_SOURCES))

DEPS             = $(CLI_DEPS) $(GUI_DEPS) $(SHARED_DEPS)

INCLUDE         := -I$(CLI_DIR)/include -I$(GUI_DIR)/include -I$(SHARED_DIR)/include

CLI_BASE_LIBS   := -lncurses

QT_LIBS          = $(shell pkg-config --libs Qt6Widgets)			# libraries (compilation time) to include
QT_CFLAGS        = -fPIC $(shell pkg-config --cflags Qt6Widgets)	# headers (building exe time) to include
GUI_BASE_LIBS    = $(QT_LIBS)

CLI_LIB_TARGET  := $(OBJ_DIR)/client/$(CLI_DIR)/$(CLI_LIB)
GUI_LIB_TARGET  := $(OBJ_DIR)/client/$(GUI_DIR)/$(GUI_LIB)

MOC              = $(shell find /usr -name moc -type f 2>/dev/null)

MOC_SRC  := $(GUI_SRC_DIR)/moc_GameWindow.cpp
MOC_OBJ  := $(OBJ_DIR)/client/gui/moc_GameWindow.o


all: cli

cli: $(LOG_DIR) $(SHARED_OBJECTS) $(CLI_LIB_TARGET)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(SHARED_OBJECTS) $(CLI_LIB_TARGET) $(CLI_BASE_LIBS) -o $(TARGET)

gui: $(LOG_DIR) $(SHARED_OBJECTS) $(GUI_LIB_TARGET)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(SHARED_OBJECTS) $(GUI_LIB_TARGET) $(GUI_BASE_LIBS) -o $(TARGET)

run: all
	./$(TARGET) $(DEFAULT_CONFIG)

$(LOG_DIR):
	mkdir -p $(LOG_DIR)


$(CLI_LIB_TARGET): $(CLI_OBJECTS)
	ar -rcs $(CLI_LIB_TARGET) $(CLI_OBJECTS)


$(GUI_LIB_TARGET): $(GUI_OBJECTS) $(MOC_OBJ)
	ar -rcs $(GUI_LIB_TARGET) $(GUI_OBJECTS) $(MOC_OBJ)

$(MOC_SRC): $(GUI_DIR)/include/GameWindow.hpp
	$(MOC) $(GUI_DIR)/include/GameWindow.hpp -o $(MOC_SRC)

$(MOC_OBJ): $(MOC_SRC)
	$(CC) $(CPP_FLAGS) $(QT_CFLAGS) -c $(MOC_SRC) -o moc_main.o


$(OBJ_DIR)/client/cli/%.o: $(CLI_SRC_DIR)/%.cpp
	@mkdir -p $(dir $@) $(dir $(DEPS_DIR)/client/cli/$*.d)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(DEPS_FLAGS) $(DEPS_DIR)/client/cli/$*.d -c $< -o $@

$(OBJ_DIR)/client/gui/%.o: $(GUI_SRC_DIR)/%.cpp
	@mkdir -p $(dir $@) $(dir $(DEPS_DIR)/client/gui/$*.d)
	$(CC) $(CPP_FLAGS) $(QT_CFLAGS) $(INCLUDE) $(DEPS_FLAGS) $(DEPS_DIR)/client/gui/$*.d -c $< -o $@

$(OBJ_DIR)/shared/%.o: $(SHARED_SRC_DIR)/%.cpp
	@mkdir -p $(dir $@) $(dir $(DEPS_DIR)/shared/$*.d)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(DEPS_FLAGS) $(DEPS_DIR)/shared/$*.d -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

fclean: clean
	rm -rf $(TARGET)

re: fclean all

.PHONY: all cli gui run clean fclean re
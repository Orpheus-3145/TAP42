CC                       := g++
DEFAULT_CONFIG           := -i 127.0.0.1 -p 4242

CPP_FLAGS                := -std=c++20 -Wall -Wextra -Werror
DEBUG_FLAGS              := -O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
DEPS_FLAGS               := -MMD -MP -MF

LOG_DIR                  := logs
CLIENT_DIR               := clients
CLI_DIR                  := cli
GUI_DIR                  := gui
BUILD_DIR                := build
SHARED_DIR_CLIENT        := shared
SHARED_DIR               := ../shared

SHARED_SRC_DIR           := $(SHARED_DIR)/source
SHARED_SRC_DIR_CLIENT    := $(SHARED_DIR_CLIENT)/source
CLI_SRC_DIR              := $(CLI_DIR)/source
GUI_SRC_DIR              := $(GUI_DIR)/source

OBJ_DIR                   = $(BUILD_DIR)/obj
SHARED_OBJ_DIR            = $(OBJ_DIR)/shared
SHARED_OBJ_DIR_CLIENT     = $(OBJ_DIR)/$(CLIENT_DIR)/shared
CLI_OBJ_DIR               = $(OBJ_DIR)/$(CLIENT_DIR)/$(CLI_DIR)
GUI_OBJ_DIR               = $(OBJ_DIR)/$(CLIENT_DIR)/$(GUI_DIR)

DEPS_DIR                  = $(BUILD_DIR)/deps
SHARED_DEPS_DIR           = $(DEPS_DIR)/shared
SHARED_DEPS_DIR_CLIENT    = $(DEPS_DIR)/$(CLIENT_DIR)/shared
CLI_DEPS_DIR              = $(DEPS_DIR)/$(CLIENT_DIR)/$(CLI_DIR)
GUI_DEPS_DIR              = $(DEPS_DIR)/$(CLIENT_DIR)/$(GUI_DIR)

SHARED_SOURCES            = $(shell find $(SHARED_SRC_DIR) -type f -name '*.cpp')
SHARED_SOURCES_CLIENT     = $(shell find $(SHARED_SRC_DIR_CLIENT) -type f -name '*.cpp')
CLI_SOURCES               = $(shell find $(CLI_SRC_DIR) -type f -name '*.cpp')
GUI_SOURCES               = $(shell find $(GUI_SRC_DIR) -type f -name '*.cpp')

SHARED_OBJECTS            = $(patsubst $(SHARED_SRC_DIR)/%.cpp,$(SHARED_OBJ_DIR)/%.o,$(SHARED_SOURCES))
SHARED_OBJECTS_CLIENT     = $(patsubst $(SHARED_SRC_DIR_CLIENT)/%.cpp,$(SHARED_OBJ_DIR_CLIENT)/%.o,$(SHARED_SOURCES_CLIENT))
CLI_OBJECTS               = $(patsubst $(CLI_SRC_DIR)/%.cpp,$(CLI_OBJ_DIR)/%.o,$(CLI_SOURCES))
GUI_OBJECTS               = $(patsubst $(GUI_SRC_DIR)/%.cpp,$(GUI_OBJ_DIR)/%.o,$(GUI_SOURCES))

SHARED_DEPS               = $(patsubst $(SHARED_SRC_DIR)/%.cpp,$(SHARED_DEPS_DIR)/%.d,$(SHARED_SOURCES))
SHARED_DEPS_CLIENT        = $(patsubst $(SHARED_SRC_DIR_CLIENT)/%.cpp,$(SHARED_DEPS_DIR_CLIENT)/%.d,$(SHARED_SOURCES_CLIENT))
CLI_DEPS                  = $(patsubst $(CLI_SRC_DIR)/%.cpp,$(CLI_DEPS_DIR)/%.d,$(CLI_SOURCES))
GUI_DEPS                  = $(patsubst $(GUI_SRC_DIR)/%.cpp,$(GUI_DEPS_DIR)/%.d,$(GUI_SOURCES))

INCLUDE                  := -I$(SHARED_DIR)/include -I$(SHARED_DIR_CLIENT)/include
INCLUDE_CLI              := $(INCLUDE) -I$(CLI_DIR)/include
INCLUDE_GUI              := $(INCLUDE) -I$(GUI_DIR)/include

CLI_BASE_LIBS            := -lncurses

GUI_QT_CFLAGS             = -fPIC $(shell pkg-config --cflags Qt6Widgets)	# headers (compilation time) to include
GUI_QT_LIBS               = $(shell pkg-config --libs Qt6Widgets)			# libraries (building exe time) to include

CLI_LIB_TARGET           := $(CLI_OBJ_DIR)/libTapCLI.a
GUI_LIB_TARGET           := $(GUI_OBJ_DIR)/libTapGUI.a

CLI_TARGET               := tap-client-cli
GUI_TARGET               := tap-client-gui
MOC                       = $(shell find /usr -name moc -type f 2>/dev/null)

MOC_SOURCES              := $(GUI_SRC_DIR)/moc_GameWindow.cpp $(GUI_SRC_DIR)/moc_GUI.cpp
MOC_OBJECTS              := $(OBJ_DIR)/$(CLIENT_DIR)/gui/moc_GameWindow.o $(OBJ_DIR)/$(CLIENT_DIR)/gui/moc_GUI.o


all: cli

cli: $(LOG_DIR) $(CLI_TARGET)

gui: $(LOG_DIR) $(GUI_TARGET)

run-cli: cli
	./$(CLI_TARGET) $(DEFAULT_CONFIG)

run-gui: gui
	./$(GUI_TARGET) $(DEFAULT_CONFIG)

$(LOG_DIR) $(SHARED_OBJ_DIR) $(SHARED_OBJ_DIR_CLIENT) $(CLI_OBJ_DIR) $(GUI_OBJ_DIR) $(SHARED_DEPS_DIR) $(SHARED_DEPS_DIR_CLIENT) $(CLI_DEPS_DIR) $(GUI_DEPS_DIR):
	mkdir -p $@

# build CLI executable
$(CLI_TARGET): $(SHARED_OBJECTS) $(SHARED_OBJECTS_CLIENT) $(CLI_LIB_TARGET) | $(LOG_DIR)
	$(CC) $(CPP_FLAGS) $(INCLUDE_CLI) $+ $(CLI_BASE_LIBS) -o $@

# build CLI library
$(CLI_LIB_TARGET): $(CLI_OBJECTS)
	ar -rcs $@ $(CLI_OBJECTS)

# compile CLI source files
$(CLI_OBJ_DIR)/%.o: $(CLI_SRC_DIR)/%.cpp | $(CLI_OBJ_DIR) $(CLI_DEPS_DIR) 
	$(CC) $(CPP_FLAGS) $(INCLUDE_CLI) $(DEPS_FLAGS) $(CLI_DEPS_DIR)/$*.d -c $< -o $@

# build GUI executable
$(GUI_TARGET): $(SHARED_OBJECTS) $(SHARED_OBJECTS_CLIENT) $(GUI_LIB_TARGET) | $(LOG_DIR)
	$(CC) $(CPP_FLAGS) $(INCLUDE_GUI) $+ $(GUI_QT_LIBS) -o $@

# build GUI library
$(GUI_LIB_TARGET): $(GUI_OBJECTS) $(MOC_OBJECTS)
	ar -rcs $@ $(GUI_OBJECTS) $(MOC_OBJECTS)

# compile GUI source files
$(GUI_OBJ_DIR)/%.o: $(GUI_SRC_DIR)/%.cpp | $(GUI_OBJ_DIR) $(GUI_DEPS_DIR)
	$(CC) $(CPP_FLAGS) $(INCLUDE_GUI) $(GUI_QT_CFLAGS) $(DEPS_FLAGS) $(GUI_DEPS_DIR)/$*.d -c $< -o $@

# build moc_ source files from Qt headers
$(GUI_SRC_DIR)/moc_%.cpp: $(GUI_DIR)/include/%.hpp
	$(MOC) $(INCLUDE_GUI) $< -o $@

# compile GUI moc_ files
$(GUI_OBJ_DIR)/moc_%.o: $(GUI_SRC_DIR)/moc_%.cpp | $(GUI_OBJ_DIR)
	$(CC) $(CPP_FLAGS) $(INCLUDE_GUI) $(GUI_QT_CFLAGS) -c $< -o $@

# compile (globally) shared source files
$(SHARED_OBJ_DIR)/%.o: $(SHARED_SRC_DIR)/%.cpp | $(SHARED_OBJ_DIR) $(SHARED_DEPS_DIR)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(DEPS_FLAGS) $(SHARED_DEPS_DIR)/$*.d -c $< -o $@

# compile (client) shared source files
$(SHARED_OBJ_DIR_CLIENT)/%.o: $(SHARED_SRC_DIR_CLIENT)/%.cpp | $(SHARED_OBJ_DIR_CLIENT) $(SHARED_DEPS_DIR_CLIENT)
	$(CC) $(CPP_FLAGS) $(INCLUDE) $(DEPS_FLAGS) $(SHARED_DEPS_DIR_CLIENT)/$*.d -c $< -o $@

-include $(CLI_DEPS) $(GUI_DEPS) $(SHARED_DEPS) $(SHARED_DEPS_CLIENT)

clean:
	rm -rf $(BUILD_DIR)

fclean: clean
	rm -rf $(CLI_TARGET) $(GUI_TARGET)

re-cli: clean cli

re-gui: clean gui

.PHONY: all cli gui run-cli run-gui clean fclean re-cli re-gui
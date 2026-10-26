CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Isrc -I../client/shared/include
SRC := $(wildcard src/*.cpp src/*/*.cpp)
OBJ := $(SRC:.cpp=.o) src/world/JsonParser.o
TARGET := tap-server

ifeq ($(OS),Windows_NT)
    LDLIBS := -lws2_32
else
    LDLIBS := -lpthread
endif

.PHONY: all deps run lint clean

all: $(TARGET)

$(TARGET): $(OBJ) src/world/JsonParser.o
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/world/JsonParser.o: ../client/shared/source/JsonParser.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

deps:
	@echo "No external dependency required: standard library only."
	@echo "World data (data/world.json) is loaded with a hand-written JSON parser (src/world/json.*)."

run: all
	./$(TARGET)

lint:
	@if command -v cppcheck > /dev/null 2>&1; then \
		cppcheck --enable=warning,style --std=c++20 --quiet --error-exitcode=1 --suppressions-list=.cppcheck-suppressions -Isrc src; \
	else echo "cppcheck not found, skipping lint"; fi

clean:
	rm -f $(OBJ) $(TARGET)
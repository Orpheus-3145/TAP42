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
	@echo "Nessuna dipendenza esterna richiesta: solo la standard library."
	@echo "Il world data (data/world.json) e' caricato con un parser JSON scritto ad hoc (src/world/json.*)."

run: all
	./$(TARGET)

lint:
	@command -v cppcheck > /dev/null 2>&1 && cppcheck --enable=warning,style --std=c++17 --quiet --error-exitcode=1 --suppressions-list=.cppcheck-suppressions -Isrc src || echo "cppcheck non trovato, salto il lint"

clean:
	rm -f $(OBJ) $(TARGET)
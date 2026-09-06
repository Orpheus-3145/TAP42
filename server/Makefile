CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Isrc
SRC := $(shell find src -name '*.cpp')
OBJ := $(SRC:.cpp=.o)
TARGET := tap-server

ifeq ($(OS),Windows_NT)
    LDLIBS := -lws2_32
else
    LDLIBS := -lpthread
endif

.PHONY: all deps run lint clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

deps:
	@echo "Nessuna dipendenza esterna richiesta: solo la standard library."
	@echo "Il world data (data/world.json) e' caricato con un parser JSON scritto ad hoc (src/world/json.*)."

run: all
	./$(TARGET)

lint:
	@which cppcheck > /dev/null 2>&1 && cppcheck --enable=warning,style --std=c++17 --quiet --error-exitcode=1 --suppressions-list=.cppcheck-suppressions -Isrc src || echo "cppcheck non trovato, salto il lint"

clean:
	rm -f $(OBJ) $(TARGET)

CXX := /usr/bin/c++

SDSL_DIR := sdsl-lite

CXXFLAGS := -std=c++17 -Wall -Wextra -O3 -DNDEBUG \
            -ffast-math -funroll-loops -msse4.2 -march=native \
            -DHAVE_CXA_DEMANGLE \
            -Iinclude -I$(SDSL_DIR)/include

LDFLAGS :=

LDLIBS :=

SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(SOURCES:.cpp=.o)

TARGET := main.x

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

test:
	bash tests/check_sdsl_v3_makefile.sh

clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -rf *.dSYM

.PHONY: all run test clean

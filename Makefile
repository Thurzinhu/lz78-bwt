CXX := /usr/bin/c++

INC_DIR := sdsl-lite/include

CXXFLAGS := -std=c++17 -Wall -Wextra -O3 \
            -ffast-math -funroll-loops -msse4.2 -march=native \
            -DHAVE_CXA_DEMANGLE \
            -Iinclude -I$(INC_DIR)

SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(SOURCES:.cpp=.o)
HEADERS := $(wildcard include/*.hpp) $(wildcard external/r-index/internal/*.hpp)
TEST_SOURCES := $(wildcard tests/*.cpp)
TEST_TARGETS := $(TEST_SOURCES:.cpp=.x)

TARGET := main.x

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

src/%.o: src/%.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	@./$(TARGET)

tests/%.x: tests/%.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

test: $(TEST_TARGETS)
	@set -e; for test in $(TEST_TARGETS); do \
		./$$test; \
	done

clean:
	rm -f $(OBJECTS) $(TARGET) $(TEST_TARGETS)
	rm -rf *.dSYM

.PHONY: all run test clean

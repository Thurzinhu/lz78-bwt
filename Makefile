CXX := /usr/bin/c++

INC_DIR := /home/arthur/include
LIB_DIR := /home/arthur/lib

CXXFLAGS := -std=c++17 -Wall -Wextra -O3 -DNDEBUG \
            -ffast-math -funroll-loops -msse4.2 -march=native \
            -DHAVE_CXA_DEMANGLE \
            -Iinclude -I$(INC_DIR)

LDFLAGS := -L$(LIB_DIR)

LDLIBS := -lsdsl -ldivsufsort -ldivsufsort64

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

clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -rf *.dSYM

.PHONY: all run clean
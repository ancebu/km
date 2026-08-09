# Makefile for Temperature Equations Unit Tests
# Pure C99, native compilation, no PIO, no frameworks

CC = gcc
CXX = g++
CFLAGS = -std=c99 -Wall -Wextra -O2 -I../include
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -I../include
LDFLAGS = -lm

# Test source
TEST_SRC = test_temperature_equations.c

# Source files to link (native-compatible implementations)
SRCS_C = \
	../src/utils/error_codes_native.cpp

SRCS_CXX = \
	../src/thermistor/thermistor_calc.cpp \
	../src/polynomial/hoge_polynomial.cpp

# Object files
OBJS_C = $(SRCS_C:.cpp=.o)
OBJS_CXX = $(SRCS_CXX:.cpp=.o)
TEST_OBJ = $(TEST_SRC:.c=.o)

TARGET = test_temperature_equations

.PHONY: all clean test run

all: $(TARGET)

$(TARGET): $(TEST_OBJ) $(OBJS_C) $(OBJS_CXX)
	$(CC) $(TEST_OBJ) $(OBJS_C) $(OBJS_CXX) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

../src/utils/%.o: ../src/utils/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

../src/thermistor/%.o: ../src/thermistor/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

../src/polynomial/%.o: ../src/polynomial/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET)
	./$(TARGET)

run: test

clean:
	rm -f $(TEST_OBJ) $(OBJS_C) $(OBJS_CXX) $(TARGET)

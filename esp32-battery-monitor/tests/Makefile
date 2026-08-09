# Makefile for Native Unit Tests
# Pure C99/C++17, native compilation, no PIO, no frameworks

CC = gcc
CXX = g++
CFLAGS = -std=c99 -Wall -Wextra -O2 -I../include -I../src
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I../include -I../src
LDFLAGS = -lm

# Test sources
TEST_C_SRC = test_temperature_equations.c
TEST_CXX_SRC = test_mux.cpp

# Source files to link (native-compatible implementations)
SRCS_C = \
	../src/utils/error_codes_native.cpp \
	../src/utils/ring_buffer.cpp

SRCS_CXX = \
	../src/thermistor/thermistor_calc.cpp \
	../src/polynomial/hoge_polynomial.cpp \
	../src/equilibrium/equilibrium_detector.cpp \
	../src/mosfet_switch/mosfet_switch.cpp \
	../src/lut/lut_calibration.cpp \
	../src/aht20/aht20_driver.cpp \
	../src/ina226/ina226_driver.cpp

# Object files
OBJS_C = $(SRCS_C:.cpp=.o)
OBJS_CXX = $(SRCS_CXX:.cpp=.o)
TEST_C_OBJ = $(TEST_C_SRC:.c=.o)
TEST_CXX_OBJ = $(TEST_CXX_SRC:.cpp=.o)

TARGET_C = test_temperature_equations
TARGET_CXX = test_mux

.PHONY: all clean test run

all: $(TARGET_C) $(TARGET_CXX)

$(TARGET_C): $(TEST_C_OBJ) $(OBJS_C) $(OBJS_CXX)
	$(CC) $(TEST_C_OBJ) $(OBJS_C) $(OBJS_CXX) -o $@ $(LDFLAGS)

$(TARGET_CXX): $(TEST_CXX_OBJ) $(OBJS_C) $(OBJS_CXX)
	$(CXX) $(TEST_CXX_OBJ) $(OBJS_C) $(OBJS_CXX) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

../src/utils/%.o: ../src/utils/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

../src/thermistor/%.o: ../src/thermistor/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

../src/polynomial/%.o: ../src/polynomial/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

../src/equilibrium/%.o: ../src/equilibrium/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

../src/mosfet_switch/%.o: ../src/mosfet_switch/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

../src/lut/%.o: ../src/lut/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

../src/aht20/%.o: ../src/aht20/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

../src/ina226/%.o: ../src/ina226/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET_C) $(TARGET_CXX)
	@echo "Running Temperature Equations Tests..."
	@./$(TARGET_C)
	@echo ""
	@echo "Running MUX Controller Tests..."
	@./$(TARGET_CXX)

run: test

clean:
	rm -f $(TEST_C_OBJ) $(TEST_CXX_OBJ) $(OBJS_C) $(OBJS_CXX) $(TARGET_C) $(TARGET_CXX)

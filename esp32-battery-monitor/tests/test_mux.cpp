/**
 * @file test_mux.cpp
 * @brief Unit tests for multiplexer controller
 */

#include <catch2/catch.hpp>
#include "types.h"
#include "config.h"

// Mock implementations for testing (no hardware)
namespace mock {
    static uint8_t g_mux_a_channel = 0;
    static uint8_t g_mux_b_channel = 0;
    static bool g_mux_enabled = false;
    
    void reset() {
        g_mux_a_channel = 0;
        g_mux_b_channel = 0;
        g_mux_enabled = false;
    }
}

TEST_CASE("Mux controller initialization", "[mux]") {
    // Test that mux controller can be initialized
    // Note: Full hardware tests require actual ESP32
    REQUIRE(CONFIG_MUX_CHANNELS_A > 0);
    REQUIRE(CONFIG_MUX_CHANNELS_B > 0);
    REQUIRE(CONFIG_MATRIX_SIZE == CONFIG_MUX_CHANNELS_A * CONFIG_MUX_CHANNELS_B);
}

TEST_CASE("Matrix coordinate conversion", "[mux][math]") {
    SECTION("Forward conversion") {
        matrix_coord_t coord = {2, 3};
        uint16_t index = matrix_coord_to_index(coord);
        REQUIRE(index == (2 * CONFIG_MATRIX_COLS) + 3);
    }
    
    SECTION("Reverse conversion") {
        uint16_t index = 11;
        matrix_coord_t coord = matrix_index_to_coord(index);
        REQUIRE(coord.row == index / CONFIG_MATRIX_COLS);
        REQUIRE(coord.col == index % CONFIG_MATRIX_COLS);
    }
    
    SECTION("Round-trip conversion") {
        for (uint8_t row = 0; row < CONFIG_MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < CONFIG_MATRIX_COLS; col++) {
                matrix_coord_t original = {row, col};
                uint16_t index = matrix_coord_to_index(original);
                matrix_coord_t result = matrix_index_to_coord(index);
                
                REQUIRE(result.row == original.row);
                REQUIRE(result.col == original.col);
            }
        }
    }
}

TEST_CASE("Error code validation", "[types]") {
    SECTION("Success code") {
        REQUIRE(IS_SUCCESS(ERR_OK));
        REQUIRE(!IS_ERROR(ERR_OK));
    }
    
    SECTION("Error codes") {
        REQUIRE(IS_ERROR(ERR_INVALID_ARG));
        REQUIRE(IS_ERROR(ERR_TIMEOUT));
        REQUIRE(IS_ERROR(ERR_NOT_INITIALIZED));
    }
}

TEST_CASE("Ring buffer operations", "[buffer]") {
    ring_buffer_t buffer;
    ring_buffer_init(&buffer);
    
    SECTION("Initial state") {
        REQUIRE(ring_buffer_is_empty(&buffer));
        REQUIRE(!ring_buffer_is_full(&buffer));
        REQUIRE(ring_buffer_count(&buffer) == 0);
    }
    
    SECTION("Push and pop") {
        battery_pack_state_t state;
        memset(&state, 0, sizeof(state));
        state.pack_voltage_v = 12.5f;
        
        REQUIRE(ring_buffer_push(&buffer, &state));
        REQUIRE(!ring_buffer_is_empty(&buffer));
        REQUIRE(ring_buffer_count(&buffer) == 1);
        
        battery_pack_state_t retrieved;
        REQUIRE(ring_buffer_pop(&buffer, &retrieved));
        REQUIRE(retrieved.pack_voltage_v == 12.5f);
        REQUIRE(ring_buffer_is_empty(&buffer));
    }
    
    SECTION("Buffer overflow handling") {
        // Fill buffer to capacity
        for (int i = 0; i < CONFIG_RING_BUFFER_SIZE + 10; i++) {
            battery_pack_state_t state;
            memset(&state, 0, sizeof(state));
            state.pack_voltage_v = 10.0f + (i * 0.1f);
            ring_buffer_push(&buffer, &state);
        }
        
        // Should not exceed capacity
        REQUIRE(ring_buffer_count(&buffer) <= CONFIG_RING_BUFFER_SIZE);
    }
}

TEST_CASE("Thermistor calibration defaults", "[thermistor]") {
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    REQUIRE(cal.beta == CONFIG_THERMISTOR_BETA);
    REQUIRE(cal.r_nominal == CONFIG_THERMISTOR_RNOM);
    REQUIRE(cal.t_nominal_c == CONFIG_THERMISTOR_TNOM);
    REQUIRE(cal.series_resistance == CONFIG_THERMISTOR_SERIES_R);
    REQUIRE(cal.gain == 1.0f);
    REQUIRE(cal.offset_c == 0.0f);
}

TEST_CASE("Temperature validation", "[thermistor]") {
    SECTION("Valid temperatures") {
        REQUIRE(thermistor_is_valid_temp(25.0f));
        REQUIRE(thermistor_is_valid_temp(0.0f));
        REQUIRE(thermistor_is_valid_temp(-20.0f));
        REQUIRE(thermistor_is_valid_temp(80.0f));
    }
    
    SECTION("Invalid temperatures") {
        REQUIRE(!thermistor_is_valid_temp(CONFIG_TEMP_MIN_C - 10.0f));
        REQUIRE(!thermistor_is_valid_temp(CONFIG_TEMP_MAX_C + 10.0f));
        REQUIRE(!thermistor_is_valid_temp(NAN));
        REQUIRE(!thermistor_is_valid_temp(INFINITY));
    }
}

TEST_CASE("Fault flag operations", "[battery]") {
    uint32_t flags = 0;
    
    SECTION("Single fault") {
        flags |= FAULT_OVERVOLTAGE;
        REQUIRE(flags & FAULT_OVERVOLTAGE);
        REQUIRE(!(flags & FAULT_UNDERVOLTAGE));
    }
    
    SECTION("Multiple faults") {
        flags |= FAULT_OVERVOLTAGE;
        flags |= FAULT_OVERCURRENT;
        flags |= FAULT_OVERTEMPERATURE;
        
        REQUIRE(flags & FAULT_OVERVOLTAGE);
        REQUIRE(flags & FAULT_OVERCURRENT);
        REQUIRE(flags & FAULT_OVERTEMPERATURE);
        REQUIRE(!(flags & FAULT_UNDERVOLTAGE));
    }
    
    SECTION("Clear faults") {
        flags = 0xFFFFFFFF;
        flags = 0;
        REQUIRE(flags == 0);
    }
}

TEST_CASE("Configuration validation", "[config]") {
    SECTION("Reasonable limits") {
        REQUIRE(CONFIG_INA226_SHUNT_R > 0.0f);
        REQUIRE(CONFIG_INA226_SHUNT_R < 1.0f);  // Less than 1 ohm
        REQUIRE(CONFIG_INA226_MAX_CURRENT > 0.0f);
        REQUIRE(CONFIG_THERMISTOR_BETA > 3000.0f);
        REQUIRE(CONFIG_THERMISTOR_BETA < 5000.0f);
    }
    
    SECTION("Safety thresholds") {
        REQUIRE(CONFIG_OVERVOLT_THRESHOLD_V > CONFIG_CELL_VOLTAGE_MIN);
        REQUIRE(CONFIG_UNDERVOLT_THRESHOLD_V < CONFIG_CELL_VOLTAGE_MAX);
        REQUIRE(CONFIG_OVERTEMP_THRESHOLD_C > 0.0f);
    }
}

/**
 * @file types.h
 * @brief Common type definitions for ESP32 Battery Monitor
 * 
 * Centralized type definitions to ensure consistency across modules.
 * No hardware dependencies - safe to include anywhere.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "config.h"

// ============================================================================
// ERROR CODES
// ============================================================================
typedef enum {
    ERR_OK = 0,
    ERR_INVALID_ARG = 1,
    ERR_TIMEOUT = 2,
    ERR_NOT_INITIALIZED = 3,
    ERR_OUT_OF_RANGE = 4,
    ERR_COMMUNICATION = 5,
    ERR_CALIBRATION_FAILED = 6,
    ERR_HARDWARE_FAULT = 7,
    ERR_MEMORY_ALLOC = 8,
    ERR_UNSUPPORTED = 9,
    ERR_SAFETY_TRIP = 10,
    ERR_UNKNOWN = 0xFF
} error_code_t;

// Convert error to string for logging
const char* error_code_to_string(error_code_t err);

// ============================================================================
// SENSOR DATA TYPES
// ============================================================================

/**
 * @brief Raw INA226 measurement data
 */
typedef struct {
    float voltage_v;      // Bus voltage in volts
    float current_a;      // Current in amperes (positive = discharge)
    float power_w;        // Power in watts
    float shunt_voltage_v;// Shunt voltage in volts
    uint16_t raw_current; // Raw current register value
    uint16_t raw_voltage; // Raw voltage register value
    uint32_t timestamp_ms;// Milliseconds since boot
} ina226_data_t;

/**
 * @brief Temperature measurement from thermistor
 */
typedef struct {
    float temperature_c;  // Temperature in Celsius
    float resistance_ohm; // Calculated resistance
    uint8_t mux_channel_a;// MUX A channel used
    uint8_t mux_channel_b;// MUX B channel used
    bool valid;           // true if measurement is valid
    uint32_t timestamp_ms;// Milliseconds since boot
} temperature_data_t;

/**
 * @brief Single cell monitoring data
 */
typedef struct {
    uint8_t cell_index;       // Cell number (0-based)
    float voltage_v;          // Cell voltage
    float temperature_c;      // Temperature at this cell
    bool balance_active;      // true if balancing is active
    uint8_t temp_sensor_id;   // Associated temperature sensor ID
} cell_data_t;

/**
 * @brief Complete battery pack state
 */
typedef struct {
    float pack_voltage_v;     // Total pack voltage
    float pack_current_a;     // Total pack current
    float pack_power_w;       // Total pack power
    float soc_percent;        // State of charge (0-100)
    float soh_percent;        // State of health (0-100)
    uint32_t cycle_count;     // Number of charge/discharge cycles
    cell_data_t cells[CONFIG_BATTERY_CELLS_SERIES];
    temperature_data_t temps[CONFIG_MATRIX_SIZE];
    uint8_t num_temp_sensors; // Number of valid temperature sensors
    bool safety_fault;        // true if any safety limit exceeded
    uint32_t fault_flags;     // Bitmask of fault conditions
    uint32_t timestamp_ms;    // Last update time
} battery_pack_state_t;

// Fault flag bitmasks
#define FAULT_OVERVOLTAGE     (1U << 0)
#define FAULT_UNDERVOLTAGE    (1U << 1)
#define FAULT_OVERCURRENT     (1U << 2)
#define FAULT_OVERTEMPERATURE (1U << 3)
#define FAULT_CELL_IMBALANCE  (1U << 4)
#define FAULT_SENSOR_FAULT    (1U << 5)
#define FAULT_COMMUNICATION   (1U << 6)

// ============================================================================
// MUX CONTROL TYPES
// ============================================================================

/**
 * @brief Multiplexer channel selection
 */
typedef struct {
    uint8_t channel_a;      // MUX A channel (0 to CONFIG_MUX_CHANNELS_A-1)
    uint8_t channel_b;      // MUX B channel (0 to CONFIG_MUX_CHANNELS_B-1)
    bool enabled;           // true if both muxes are enabled
} mux_selection_t;

/**
 * @brief Matrix coordinate for thermistor array
 */
typedef struct {
    uint8_t row;            // Row index (MUX A)
    uint8_t col;            // Column index (MUX B)
} matrix_coord_t;

// Convert matrix coordinate to linear index
static inline uint16_t matrix_coord_to_index(matrix_coord_t coord) {
    return (coord.row * CONFIG_MATRIX_COLS) + coord.col;
}

// Convert linear index to matrix coordinate
static inline matrix_coord_t matrix_index_to_coord(uint16_t index) {
    matrix_coord_t coord;
    coord.row = index / CONFIG_MATRIX_COLS;
    coord.col = index % CONFIG_MATRIX_COLS;
    return coord;
}

// ============================================================================
// CALIBRATION DATA
// ============================================================================

/**
 * @brief INA226 calibration constants
 */
typedef struct {
    float calibration_factor;  // Current LSB calibration
    float shunt_resistance;    // Actual shunt resistance (Ω)
    float current_lsb;         // Current per bit (A/bit)
    float voltage_lsb;         // Voltage per bit (V/bit)
    bool calibrated;           // true if calibration is valid
} ina226_calibration_t;

/**
 * @brief Thermistor calibration data
 */
typedef struct {
    float beta;                // Beta parameter
    float r_nominal;           // Nominal resistance (Ω)
    float t_nominal_c;         // Nominal temperature (°C)
    float series_resistance;   // Series resistor (Ω)
    float offset_c;            // Temperature offset correction
    float gain;                // Temperature gain correction
} thermistor_calibration_t;

// ============================================================================
// RING BUFFER FOR DATA LOGGING
// ============================================================================

typedef struct {
    battery_pack_state_t data[CONFIG_RING_BUFFER_SIZE];
    uint16_t head;          // Write index
    uint16_t tail;          // Read index
    uint16_t count;         // Number of items in buffer
    bool full;              // true if buffer is full
} ring_buffer_t;

#ifdef __cplusplus
extern "C" {
#endif

// Ring buffer operations
void ring_buffer_init(ring_buffer_t* buffer);
bool ring_buffer_push(ring_buffer_t* buffer, const battery_pack_state_t* data);
bool ring_buffer_pop(ring_buffer_t* buffer, battery_pack_state_t* data);
uint16_t ring_buffer_count(const ring_buffer_t* buffer);
bool ring_buffer_is_full(const ring_buffer_t* buffer);
bool ring_buffer_is_empty(const ring_buffer_t* buffer);

#ifdef __cplusplus
}
#endif

// ============================================================================
// CALLBACK TYPES
// ============================================================================

/**
 * @brief Callback function type for sensor updates
 */
typedef void (*sensor_update_callback_t)(const battery_pack_state_t* state, void* user_data);

/**
 * @brief Callback function type for fault events
 */
typedef void (*fault_callback_t)(uint32_t fault_flags, void* user_data);

// ============================================================================
// UTILITY MACROS
// ============================================================================

// Check if error code indicates success
#define IS_SUCCESS(err) ((err) == ERR_OK)

// Check if error code indicates failure
#define IS_ERROR(err) ((err) != ERR_OK)

// Get minimum of two values
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

// Get maximum of two values
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// Clamp value between min and max
#define CLAMP(val, min_val, max_val) MIN(MAX(val, min_val), max_val)

// Array size macro
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Unused parameter macro (avoid compiler warnings)
#define UNUSED(x) (void)(x)

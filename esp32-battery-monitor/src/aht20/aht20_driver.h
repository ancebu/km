/**
 * @file aht20_driver.h
 * @brief AHT20 Temperature & Humidity Sensor Driver
 * 
 * Reference sensor for self-calibration framework.
 * Provides absolute temperature reference for NTC calibration during thermal equilibrium.
 * 
 * Datasheet: ±0.3°C typical accuracy, used as ground truth for opportunistic calibration.
 */

#pragma once

#include "types.h"
#include "config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// AHT20 CONFIGURATION
// ============================================================================
#define AHT20_DEFAULT_ADDRESS       0x38    // Fixed I2C address
#define AHT20_CMD_INITIALIZE        0xBE    // Initialization command
#define AHT20_CMD_TRIGGER           0xAC    // Measurement trigger
#define AHT20_CMD_RESET             0xBA    // Soft reset
#define AHT20_STATUS_BUSY           0x80    // Status register busy bit
#define AHT20_STATUS_CALIBRATED     0x08    // Status register calibrated bit

// Calibration thresholds from PDF
#define AHT20_TEMP_ACCURACY_TYPICAL 0.3f    // ±0.3°C typical
#define AHT20_HUMIDITY_ACCURACY     2.0f    // ±2% RH typical

// ============================================================================
// DATA TYPES
// ============================================================================

/**
 * @brief AHT20 measurement data
 */
typedef struct {
    float temperature_c;      // Temperature in Celsius
    float humidity_percent;   // Relative humidity 0-100%
    uint8_t status;           // Status register value
    bool valid;               // true if measurement is valid
    uint32_t timestamp_ms;    // Milliseconds since boot
} aht20_data_t;

/**
 * @brief AHT20 driver state
 */
typedef struct {
    bool initialized;         // true if sensor is initialized
    uint8_t i2c_address;      // I2C address
    float temp_offset;        // Temperature offset calibration
    float humidity_offset;    // Humidity offset calibration
    uint32_t last_read_ms;    // Last successful read timestamp
    uint8_t error_count;      // Consecutive error count
    // Derivative tracking for dT/dt calculation (moved from globals)
    float last_temp_reading;  // Previous temperature reading
    uint32_t last_temp_time_ms; // Timestamp of previous reading
} aht20_state_t;

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Initialize AHT20 sensor
 * @param state Pointer to driver state structure
 * @return ERR_OK on success, error code otherwise
 */
error_code_t aht20_init(aht20_state_t* state);

/**
 * @brief Trigger and read temperature/humidity measurement
 * @param state Pointer to driver state structure
 * @param data Output: measurement data
 * @return ERR_OK on success, error code otherwise
 * 
 * @note Blocking call - takes ~80ms for measurement
 */
error_code_t aht20_read(aht20_state_t* state, aht20_data_t* data);

/**
 * @brief Non-blocking read (uses cached data if sensor busy)
 * @param state Pointer to driver state structure
 * @param data Output: measurement data
 * @return ERR_OK on success, error code otherwise
 */
error_code_t aht20_read_nonblocking(aht20_state_t* state, aht20_data_t* data);

/**
 * @brief Calculate temperature derivative (dT/dt)
 * @param state Pointer to driver state structure
 * @param new_temp New temperature reading
 * @return Temperature rate of change in °C/min
 * 
 * Used for thermal equilibrium detection per PDF spec.
 */
float aht20_calculate_dt_dmin(aht20_state_t* state, float new_temp);

/**
 * @brief Reset AHT20 sensor
 * @param state Pointer to driver state structure
 * @return ERR_OK on success, error code otherwise
 */
error_code_t aht20_reset(aht20_state_t* state);

/**
 * @brief Check if AHT20 is calibrated (factory calibration status)
 * @param state Pointer to driver state structure
 * @return true if sensor reports factory calibrated
 */
bool aht20_is_calibrated(aht20_state_t* state);

/**
 * @brief Apply user calibration offset to temperature readings
 * @param state Pointer to driver state structure
 * @param offset_c Offset in Celsius to apply
 */
void aht20_set_temp_offset(aht20_state_t* state, float offset_c);

/**
 * @brief Get current temperature offset
 * @param state Pointer to driver state structure
 * @return Current offset in Celsius
 */
float aht20_get_temp_offset(const aht20_state_t* state);

#ifdef __cplusplus
}
#endif

/**
 * @file equilibrium_detector.h
 * @brief Thermal Equilibrium Detection for Self-Calibration Framework
 * 
 * Implements robust logic to detect periods of thermal equilibrium from continuous telemetry.
 * Per PDF spec: equilibrium = zero load + stable temp + minimal gradient across all sensors.
 */

#pragma once

#include "types.h"
#include "config.h"
#include "aht20/aht20_driver.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// EQUILIBRIUM THRESHOLDS (from PDF specification)
// ============================================================================
#define EQUILIBRIUM_CURRENT_THRESHOLD_mA    200     // < 200mA for parasitic standby
#define EQUILIBRIUM_DT_THRESHOLD_C_PER_MIN  0.05f   // dT/dt < 0.05 °C/min
#define EQUILIBRIUM_SPREAD_THRESHOLD_C      0.1f    // Sensor spread < 0.1°C
#define EQUILIBRIUM_STABILITY_COUNT         10      // Consecutive stable readings required

// ============================================================================
// DATA TYPES
// ============================================================================

/**
 * @brief Equilibrium detection state machine
 */
typedef struct {
    bool in_equilibrium;              // true if currently in equilibrium state
    uint32_t equilibrium_start_ms;    // Timestamp when equilibrium was entered
    uint32_t equilibrium_duration_ms; // Total time spent in current equilibrium
    uint16_t stable_reading_count;    // Consecutive stable readings
    float reference_temp_c;           // AHT20 temperature at equilibrium start
    float equilibrium_temp_avg_c;     // Average temperature during equilibrium
    uint32_t samples_collected;       // Number of calibration points collected in this session
    uint32_t last_dt_check_time_ms;   // for dT/dt derivative only
    float last_dt_check_temp_c;       // for dT/dt derivative only
} equilibrium_state_t;

/**
 * @brief Equilibrium detection configuration
 */
typedef struct {
    float current_threshold_a;        // Current threshold in Amps
    float dt_threshold_c_per_min;     // Temperature derivative threshold
    float spread_threshold_c;         // Max temperature spread between sensors
    uint16_t stability_count;         // Required consecutive stable readings
    float min_equilibrium_duration_s; // Minimum duration to consider valid (seconds)
} equilibrium_config_t;

/**
 * @brief Calibration data point harvested during equilibrium
 */
typedef struct {
    float ntc_resistance_ohm;       // Measured NTC resistance
    float aht20_temperature_c;      // Reference temperature from AHT20
    uint8_t ntc_channel_id;         // Which NTC channel this belongs to
    uint32_t timestamp_ms;          // When this sample was taken
    float confidence_score;         // Statistical confidence (based on stability duration)
} calibration_point_t;

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Initialize equilibrium detector
 * @param state Pointer to equilibrium state structure
 * @param config Configuration parameters (NULL for defaults)
 * @return ERR_OK on success, error code otherwise
 */
error_code_t equilibrium_init(equilibrium_state_t* state, const equilibrium_config_t* config);

/**
 * @brief Check if system is in thermal equilibrium
 * @param state Pointer to equilibrium state structure
 * @param current_a Measured pack current in Amps
 * @param aht20_temp_c AHT20 reference temperature
 * @param ntc_temps Array of NTC temperatures (size = CONFIG_MATRIX_SIZE)
 * @param num_ntc_sensors Number of valid NTC sensors
 * @return true if in equilibrium state
 * 
 * This is the main detection function called each sampling cycle.
 * Monitors: current, dT/dt, and sensor spread simultaneously.
 */
bool equilibrium_check(equilibrium_state_t* state,
                       float current_a,
                       float aht20_temp_c,
                       const float* ntc_temps,
                       uint8_t num_ntc_sensors);

/**
 * @brief Get equilibrium duration in milliseconds
 * @param state Pointer to equilibrium state structure
 * @return Duration in ms, or 0 if not in equilibrium
 */
uint32_t equilibrium_get_duration_ms(const equilibrium_state_t* state);

/**
 * @brief Get number of calibration points collected during current equilibrium
 * @param state Pointer to equilibrium state structure
 * @return Number of samples collected
 */
uint32_t equilibrium_get_sample_count(const equilibrium_state_t* state);

/**
 * @brief Harvest a calibration data point during equilibrium
 * @param state Pointer to equilibrium state structure
 * @param ntc_resistance_ohm Measured NTC resistance
 * @param aht20_temp_c Reference temperature from AHT20
 * @param ntc_channel_id Which NTC channel this belongs to
 * @param point Output: harvested calibration point
 * @return ERR_OK if point harvested, ERR_NOT_INITIALIZED if not in equilibrium
 */
error_code_t equilibrium_harvest_point(equilibrium_state_t* state,
                                        float ntc_resistance_ohm,
                                        float aht20_temp_c,
                                        uint8_t ntc_channel_id,
                                        calibration_point_t* point);

/**
 * @brief Calculate confidence score for calibration data
 * @param state Pointer to equilibrium state structure
 * @return Confidence score 0.0 to 1.0
 * 
 * Based on equilibrium duration and stability metrics.
 * Longer stable periods = higher confidence.
 */
float equilibrium_calculate_confidence(const equilibrium_state_t* state);

/**
 * @brief Reset equilibrium state machine
 * @param state Pointer to equilibrium state structure
 */
void equilibrium_reset(equilibrium_state_t* state);

/**
 * @brief Get default equilibrium configuration
 * @param config Output: default configuration
 */
void equilibrium_get_default_config(equilibrium_config_t* config);

#ifdef __cplusplus
}
#endif

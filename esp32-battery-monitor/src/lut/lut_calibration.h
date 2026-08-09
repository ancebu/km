/**
 * @file lut_calibration.h
 * @brief Look-Up Table with Linear Interpolation for Temperature Calibration
 * 
 * Stores empirically measured (Resistance, Temperature) pairs from thermal equilibrium states.
 * Uses linear interpolation between calibrated points to estimate temperature within range.
 */

#pragma once

#include "types.h"
#include "config.h"
#include "equilibrium/equilibrium_detector.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// LUT CONFIGURATION
// ============================================================================
#define LUT_DEFAULT_BIN_COUNT       256     // Number of temperature bins
#define LUT_MIN_TEMP_C              -40.0f  // Minimum calibrated temperature
#define LUT_MAX_TEMP_C              125.0f  // Maximum calibrated temperature
#define LUT_MIN_SAMPLES_PER_BIN     3       // Minimum samples before bin is valid
#define LUT_CONFIDENCE_THRESHOLD    0.5f    // Minimum confidence to use bin

// Temperature range span
#define LUT_TEMP_SPAN_C             (LUT_MAX_TEMP_C - LUT_MIN_TEMP_C)

// ============================================================================
// DATA TYPES
// ============================================================================

/**
 * @brief Single LUT bin storing calibration data
 */
typedef struct {
    float center_temp_c;        // Center temperature of this bin
    float avg_resistance_ohm;   // Average resistance at this temperature
    float running_sum_r;        // Running sum for averaging
    float running_sum_t;        // Running sum for temperature
    uint16_t sample_count;      // Number of samples accumulated
    bool valid;                 // true if bin has enough samples
    float confidence;           // Confidence score based on sample count and stability
} lut_bin_t;

/**
 * @brief Complete LUT structure for one NTC channel
 */
typedef struct {
    lut_bin_t* bins;            // Array of LUT bins
    uint16_t bin_count;         // Number of bins
    float min_temp_c;           // Minimum calibrated temperature
    float max_temp_c;           // Maximum calibrated temperature
    uint8_t channel_id;         // Which NTC channel this LUT belongs to
    uint32_t last_update_ms;    // Last time LUT was updated
    bool initialized;           // true if LUT is ready for use
} lut_table_t;

/**
 * @brief Cubic spline coefficients for interpolation
 */
typedef struct {
    float a;  // Coefficient for (x - xi)^0
    float b;  // Coefficient for (x - xi)^1
    float c;  // Coefficient for (x - xi)^2
    float d;  // Coefficient for (x - xi)^3
} spline_coefficient_t;

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Initialize LUT for a specific NTC channel
 * @param lut Pointer to LUT table structure
 * @param channel_id Which NTC channel (0 to CONFIG_MATRIX_SIZE-1)
 * @param bin_count Number of bins (or 0 for default)
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lut_init(lut_table_t* lut, uint8_t channel_id, uint16_t bin_count);

/**
 * @brief Add a calibration point to the LUT
 * @param lut Pointer to LUT table structure
 * @param point Calibration point harvested during equilibrium
 * @return ERR_OK on success, error code otherwise
 * 
 * Updates running averages and marks bin as valid when enough samples collected.
 */
error_code_t lut_add_point(lut_table_t* lut, const calibration_point_t* point);

/**
 * @brief Calculate temperature from resistance using linear interpolation
 * @param lut Pointer to LUT table structure
 * @param resistance_ohm Measured resistance in ohms
 * @param temp_c_output Output: interpolated temperature in Celsius
 * @return ERR_OK if within calibrated range, ERR_OUT_OF_RANGE if extrapolation needed
 * 
 * Uses linear interpolation between the two nearest valid bins.
 * Returns error if resistance is outside calibrated bounds (use polynomial instead).
 */
error_code_t lut_interp_temp(const lut_table_t* lut, float resistance_ohm, float* temp_c_output);

/**
 * @brief Check if resistance is within calibrated LUT range
 * @param lut Pointer to LUT table structure
 * @param resistance_ohm Resistance to check
 * @return true if within calibrated range
 */
bool lut_is_in_range(const lut_table_t* lut, float resistance_ohm);

/**
 * @brief Get the calibrated temperature range
 * @param lut Pointer to LUT table structure
 * @param min_temp_output Output: minimum calibrated temperature
 * @param max_temp_output Output: maximum calibrated temperature
 * @return ERR_OK if LUT is initialized
 */
error_code_t lut_get_range(const lut_table_t* lut, float* min_temp_output, float* max_temp_output);

/**
 * @brief Build cubic spline coefficients from LUT data (not implemented)
 * @param lut Pointer to LUT table structure
 * @param coefficients_output Output: array of spline coefficients
 * @param num_coefficients_output Output: number of valid coefficients
 * @return ERR_UNSUPPORTED (not implemented - uses linear interpolation instead)
 * 
 * This function is not implemented. The LUT uses linear interpolation between bins.
 */
error_code_t lut_build_spline(const lut_table_t* lut,
                               spline_coefficient_t** coefficients_output,
                               uint16_t* num_coefficients_output);

/**
 * @brief Get statistics about LUT coverage
 * @param lut Pointer to LUT table structure
 * @param total_bins Output: total number of bins
 * @param valid_bins Output: number of bins with sufficient samples
 * @param total_samples Output: total samples across all bins
 * @return ERR_OK on success
 */
error_code_t lut_get_stats(const lut_table_t* lut,
                           uint16_t* total_bins,
                           uint16_t* valid_bins,
                           uint32_t* total_samples);

/**
 * @brief Clear all data from LUT and reset
 * @param lut Pointer to LUT table structure
 */
void lut_reset(lut_table_t* lut);

/**
 * @brief Merge calibration data from another LUT (for multi-session calibration)
 * @param dest Destination LUT
 * @param source Source LUT to merge from
 * @return ERR_OK on success
 */
error_code_t lut_merge(lut_table_t* dest, const lut_table_t* source);

/**
 * @brief Export LUT data for storage in NVS/Flash
 * @param lut Pointer to LUT table structure
 * @param buffer Output: buffer to store serialized data
 * @param buffer_size Size of buffer in bytes
 * @param bytes_written Output: number of bytes written
 * @return ERR_OK on success
 */
error_code_t lut_serialize(const lut_table_t* lut, uint8_t* buffer, size_t buffer_size, size_t* bytes_written);

/**
 * @brief Import LUT data from NVS/Flash
 * @param lut Pointer to LUT table structure
 * @param buffer Input: buffer containing serialized data
 * @param buffer_size Size of buffer in bytes
 * @return ERR_OK on success
 */
error_code_t lut_deserialize(lut_table_t* lut, const uint8_t* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

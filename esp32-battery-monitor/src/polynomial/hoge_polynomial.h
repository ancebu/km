/**
 * @file hoge_polynomial.h
 * @brief 4th-Order Hoge Polynomial for Temperature Extrapolation
 * 
 * Implements the Hoge equation: 1/T = A0 + A1*ln(R) + A2*ln(R)^2 + A3*ln(R)^3 + A4*ln(R)^4
 * Used for robust extrapolation outside the calibrated LUT range.
 * 
 * Per PDF: Higher-order polynomials outperform standard Steinhart-Hart over wide spans.
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
// POLYNOMIAL CONFIGURATION
// ============================================================================
#define HOGE_POLY_ORDER         4       // 4th order polynomial
#define HOGE_NUM_COEFFICIENTS   5       // A0 through A4
#define HOGE_PHANTOM_ANCHOR_WEIGHT_LOW  0.1f   // Weight for -20°C phantom anchor
#define HOGE_PHANTOM_ANCHOR_WEIGHT_HIGH 0.1f   // Weight for 100°C phantom anchor

// Phantom anchor temperatures (from PDF spec)
#define HOGE_PHANTOM_TEMP_MIN_C   -20.0f
#define HOGE_PHANTOM_TEMP_MAX_C   100.0f

// Refit thresholds
#define HOGE_REFIT_SAMPLE_THRESHOLD_POWER_OF_10  500  // Refit when samples cross power-of-10

// ============================================================================
// DATA TYPES
// ============================================================================

/**
 * @brief Hoge polynomial coefficients
 */
typedef struct {
    float A[HOGE_NUM_COEFFICIENTS];  // Coefficients A0 through A4
    bool valid;                       // true if coefficients are valid
    uint32_t last_refit_timestamp;    // When coefficients were last updated
    uint32_t samples_used;            // Number of samples used in last fit
} hoge_polynomial_t;

/**
 * @brief Calibration data for polynomial fitting
 */
typedef struct {
    float ln_resistance;      // Natural log of resistance
    float inverse_temp_k;     // 1/T in Kelvin^-1
    float weight;             // Sample weight (for weighted least squares)
} poly_calibration_sample_t;

/**
 * @brief Polynomial fitting state
 */
typedef struct {
    hoge_polynomial_t polynomial;
    poly_calibration_sample_t* samples;  // Array of calibration samples
    uint32_t sample_count;               // Number of samples collected
    uint32_t max_samples;                // Maximum samples to store
    bool needs_refit;                    // true if polynomial should be refitted
    uint32_t last_refit_sample_count;    // Sample count at last refit
} poly_fit_state_t;

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Initialize Hoge polynomial calculator
 * @param state Pointer to polynomial state structure
 * @param max_samples Maximum number of calibration samples to store
 * @return ERR_OK on success, error code otherwise
 */
error_code_t hoge_polynomial_init(poly_fit_state_t* state, uint32_t max_samples);

/**
 * @brief Calculate temperature from resistance using current polynomial
 * @param state Pointer to polynomial state structure
 * @param resistance_ohm Measured resistance in ohms
 * @param temp_c_output Output: calculated temperature in Celsius
 * @return ERR_OK on success, error code if polynomial invalid
 * 
 * Evaluates: T = 1 / (A0 + A1*lnR + A2*lnR^2 + A3*lnR^3 + A4*lnR^4)
 */
error_code_t hoge_polynomial_calc_temp(const poly_fit_state_t* state,
                                        float resistance_ohm,
                                        float* temp_c_output);

/**
 * @brief Add a calibration sample for polynomial fitting
 * @param state Pointer to polynomial state structure
 * @param resistance_ohm Measured resistance
 * @param temp_c Measured temperature (from AHT20 reference)
 * @param confidence Confidence score 0.0 to 1.0 (used as weight)
 * @return ERR_OK on success, ERR_MEMORY_ALLOC if buffer full
 * 
 * Triggers refit if: new LUT bin populated OR sample count crosses power-of-10 threshold
 */
error_code_t hoge_polynomial_add_sample(poly_fit_state_t* state,
                                         float resistance_ohm,
                                         float temp_c,
                                         float confidence);

/**
 * @brief Fit polynomial to collected calibration data
 * @param state Pointer to polynomial state structure
 * @return ERR_OK on success, error code otherwise
 * 
 * Uses weighted least-squares regression with phantom anchors.
 * Runs in milliseconds on ESP32 - suitable for background task.
 */
error_code_t hoge_polynomial_fit(poly_fit_state_t* state);

/**
 * @brief Calculate phantom anchor points from NTC datasheet parameters
 * @param beta Beta parameter from NTC datasheet
 * @param r_nominal Nominal resistance at Tnom
 * @param t_nominal_c Nominal temperature in Celsius
 * @param anchor_min_output Output: low temperature anchor (lnR, 1/T)
 * @param anchor_high_output Output: high temperature anchor (lnR, 1/T)
 */
void hoge_calculate_phantom_anchors(float beta,
                                     float r_nominal,
                                     float t_nominal_c,
                                     poly_calibration_sample_t* anchor_min_output,
                                     poly_calibration_sample_t* anchor_high_output);

/**
 * @brief Check if polynomial needs refitting
 * @param state Pointer to polynomial state structure
 * @param current_sample_count Current number of calibration samples
 * @return true if refit is recommended
 */
bool hoge_polynomial_needs_refit(const poly_fit_state_t* state, uint32_t current_sample_count);

/**
 * @brief Get polynomial coefficients for external use
 * @param state Pointer to polynomial state structure
 * @param coefficients_output Output: array of 5 coefficients
 * @return ERR_OK if valid, ERR_NOT_INITIALIZED otherwise
 */
error_code_t hoge_polynomial_get_coefficients(const poly_fit_state_t* state,
                                               float coefficients_output[HOGE_NUM_COEFFICIENTS]);

/**
 * @brief Clear all calibration samples and reset polynomial
 * @param state Pointer to polynomial state structure
 */
void hoge_polynomial_reset(poly_fit_state_t* state);

/**
 * @brief Evaluate if resistance is within calibrated LUT range
 * @param state Pointer to polynomial state structure
 * @param resistance_ohm Resistance to check
 * @param min_resistance_output Output: minimum calibrated resistance
 * @param max_resistance_output Output: maximum calibrated resistance
 * @return true if within range, false if extrapolation needed
 */
bool hoge_polynomial_check_range(const poly_fit_state_t* state,
                                  float resistance_ohm,
                                  float* min_resistance_output,
                                  float* max_resistance_output);

#ifdef __cplusplus
}
#endif

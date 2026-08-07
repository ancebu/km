/**
 * @file hoge_polynomial.cpp
 * @brief 4th-Order Hoge Polynomial Implementation
 */

#include "hoge_polynomial.h"
#include <Arduino.h>
#include <math.h>
#include <stdlib.h>

// ============================================================================
// MATRIX OPERATIONS FOR LEAST SQUARES FIT
// ============================================================================

// Solve Ax = B using Gaussian elimination for small matrices (5x5 max)
static bool solve_linear_system(float A[HOGE_NUM_COEFFICIENTS][HOGE_NUM_COEFFICIENTS],
                                 float B[HOGE_NUM_COEFFICIENTS],
                                 float X[HOGE_NUM_COEFFICIENTS]) {
    int n = HOGE_NUM_COEFFICIENTS;
    
    // Forward elimination
    for (int i = 0; i < n; i++) {
        // Find pivot
        int max_row = i;
        for (int k = i + 1; k < n; k++) {
            if (fabsf(A[k][i]) > fabsf(A[max_row][i])) {
                max_row = k;
            }
        }
        
        // Swap rows
        if (max_row != i) {
            for (int j = 0; j < n; j++) {
                float temp = A[i][j];
                A[i][j] = A[max_row][j];
                A[max_row][j] = temp;
            }
            float temp = B[i];
            B[i] = B[max_row];
            B[max_row] = temp;
        }
        
        // Check for singular matrix
        if (fabsf(A[i][i]) < 1e-10f) {
            return false;
        }
        
        // Eliminate column
        for (int k = i + 1; k < n; k++) {
            float factor = A[k][i] / A[i][i];
            for (int j = i; j < n; j++) {
                A[k][j] -= factor * A[i][j];
            }
            B[k] -= factor * B[i];
        }
    }
    
    // Back substitution
    for (int i = n - 1; i >= 0; i--) {
        X[i] = B[i];
        for (int j = i + 1; j < n; j++) {
            X[i] -= A[i][j] * X[j];
        }
        X[i] /= A[i][i];
    }
    
    return true;
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

error_code_t hoge_polynomial_init(poly_fit_state_t* state, uint32_t max_samples) {
    if (!state) {
        return ERR_INVALID_ARG;
    }
    
    memset(state, 0, sizeof(poly_fit_state_t));
    
    // Allocate sample buffer
    if (max_samples > 0) {
        state->samples = (poly_calibration_sample_t*)calloc(max_samples, sizeof(poly_calibration_sample_t));
        if (!state->samples) {
            return ERR_MEMORY_ALLOC;
        }
        state->max_samples = max_samples;
    } else {
        state->samples = NULL;
        state->max_samples = 0;
    }
    
    // Initialize polynomial with default values (will be invalid until first fit)
    state->polynomial.valid = false;
    state->needs_refit = false;
    
    return ERR_OK;
}

error_code_t hoge_polynomial_calc_temp(const poly_fit_state_t* state,
                                        float resistance_ohm,
                                        float* temp_c_output) {
    if (!state || !temp_c_output) {
        return ERR_INVALID_ARG;
    }
    
    if (!state->polynomial.valid) {
        return ERR_NOT_INITIALIZED;
    }
    
    if (resistance_ohm <= 0.0f) {
        return ERR_OUT_OF_RANGE;
    }
    
    // Calculate ln(R)
    float ln_r = logf(resistance_ohm);
    
    // Evaluate polynomial: 1/T = A0 + A1*lnR + A2*lnR^2 + A3*lnR^3 + A4*lnR^4
    const float* A = state->polynomial.A;
    float inverse_t = A[0] + 
                      A[1] * ln_r + 
                      A[2] * ln_r * ln_r + 
                      A[3] * ln_r * ln_r * ln_r + 
                      A[4] * ln_r * ln_r * ln_r * ln_r;
    
    // Check for invalid result
    if (inverse_t <= 0.0f) {
        return ERR_OUT_OF_RANGE;
    }
    
    // Convert to temperature: T(K) = 1 / (1/T), then to Celsius
    float temp_k = 1.0f / inverse_t;
    *temp_c_output = temp_k - 273.15f;
    
    return ERR_OK;
}

error_code_t hoge_polynomial_add_sample(poly_fit_state_t* state,
                                         float resistance_ohm,
                                         float temp_c,
                                         float confidence) {
    if (!state) {
        return ERR_INVALID_ARG;
    }
    
    if (resistance_ohm <= 0.0f) {
        return ERR_OUT_OF_RANGE;
    }
    
    if (state->sample_count >= state->max_samples) {
        return ERR_MEMORY_ALLOC;
    }
    
    // Convert to fitting parameters
    state->samples[state->sample_count].ln_resistance = logf(resistance_ohm);
    state->samples[state->sample_count].inverse_temp_k = 1.0f / (temp_c + 273.15f);
    state->samples[state->sample_count].weight = fmaxf(confidence, 0.01f);  // Minimum weight
    
    state->sample_count++;
    
    // Check if refit is needed
    if (hoge_polynomial_needs_refit(state, state->sample_count)) {
        state->needs_refit = true;
    }
    
    return ERR_OK;
}

error_code_t hoge_polynomial_fit(poly_fit_state_t* state) {
    if (!state) {
        return ERR_INVALID_ARG;
    }
    
    if (state->sample_count < HOGE_NUM_COEFFICIENTS) {
        return ERR_OUT_OF_RANGE;  // Not enough samples
    }
    
    // Build normal equations: (X^T W X) * A = X^T W Y
    // Where X is the Vandermonde matrix of ln(R) values
    
    float XtWX[HOGE_NUM_COEFFICIENTS][HOGE_NUM_COEFFICIENTS] = {0};
    float XtWY[HOGE_NUM_COEFFICIENTS] = {0};
    
    // Add phantom anchors with low weight
    poly_calibration_sample_t phantom_low, phantom_high;
    hoge_calculate_phantom_anchors(CONFIG_THERMISTOR_BETA,
                                    CONFIG_THERMISTOR_RNOM,
                                    CONFIG_THERMISTOR_TNOM,
                                    &phantom_low,
                                    &phantom_high);
    
    // Process all samples including phantom anchors
    poly_calibration_sample_t all_samples[HOGE_REFIT_SAMPLE_THRESHOLD_POWER_OF_10 + 2];
    uint32_t total_samples = state->sample_count;
    
    // Copy real samples
    for (uint32_t i = 0; i < state->sample_count; i++) {
        all_samples[i] = state->samples[i];
    }
    
    // Add phantom anchors
    all_samples[total_samples] = phantom_low;
    all_samples[total_samples + 1] = phantom_high;
    total_samples += 2;
    
    // Build normal equations
    for (uint32_t s = 0; s < total_samples; s++) {
        float ln_r = all_samples[s].ln_resistance;
        float inv_t = all_samples[s].inverse_temp_k;
        float w = all_samples[s].weight;
        
        // Build powers of ln(R): [1, lnR, lnR^2, lnR^3, lnR^4]
        float powers[HOGE_NUM_COEFFICIENTS];
        powers[0] = 1.0f;
        for (int i = 1; i < HOGE_NUM_COEFFICIENTS; i++) {
            powers[i] = powers[i-1] * ln_r;
        }
        
        // Accumulate X^T W X
        for (int i = 0; i < HOGE_NUM_COEFFICIENTS; i++) {
            for (int j = 0; j < HOGE_NUM_COEFFICIENTS; j++) {
                XtWX[i][j] += w * powers[i] * powers[j];
            }
            // Accumulate X^T W Y
            XtWY[i] += w * powers[i] * inv_t;
        }
    }
    
    // Solve linear system
    float coefficients[HOGE_NUM_COEFFICIENTS];
    if (!solve_linear_system(XtWX, XtWY, coefficients)) {
        return ERR_CALIBRATION_FAILED;
    }
    
    // Store results
    for (int i = 0; i < HOGE_NUM_COEFFICIENTS; i++) {
        state->polynomial.A[i] = coefficients[i];
    }
    
    state->polynomial.valid = true;
    state->polynomial.last_refit_timestamp = millis();
    state->polynomial.samples_used = state->sample_count;
    state->needs_refit = false;
    state->last_refit_sample_count = state->sample_count;
    
    return ERR_OK;
}

void hoge_calculate_phantom_anchors(float beta,
                                     float r_nominal,
                                     float t_nominal_c,
                                     poly_calibration_sample_t* anchor_min_output,
                                     poly_calibration_sample_t* anchor_high_output) {
    if (!anchor_min_output || !anchor_high_output) {
        return;
    }
    
    // Calculate resistance at phantom temperatures using Beta equation
    float t_min_k = HOGE_PHANTOM_TEMP_MIN_C + 273.15f;
    float t_max_k = HOGE_PHANTOM_TEMP_MAX_C + 273.15f;
    float t_nom_k = t_nominal_c + 273.15f;
    
    // R(T) = R_nom * exp(Beta * (1/T - 1/T_nom))
    float r_min = r_nominal * expf(beta * (1.0f/t_min_k - 1.0f/t_nom_k));
    float r_max = r_nominal * expf(beta * (1.0f/t_max_k - 1.0f/t_nom_k));
    
    // Populate anchors with low weight
    anchor_min_output->ln_resistance = logf(r_min);
    anchor_min_output->inverse_temp_k = 1.0f / t_min_k;
    anchor_min_output->weight = HOGE_PHANTOM_ANCHOR_WEIGHT_LOW;
    
    anchor_high_output->ln_resistance = logf(r_max);
    anchor_high_output->inverse_temp_k = 1.0f / t_max_k;
    anchor_high_output->weight = HOGE_PHANTOM_ANCHOR_WEIGHT_HIGH;
}

bool hoge_polynomial_needs_refit(const poly_fit_state_t* state, uint32_t current_sample_count) {
    if (!state) {
        return false;
    }
    
    // Refit if this is the first fit
    if (!state->polynomial.valid) {
        return current_sample_count >= HOGE_NUM_COEFFICIENTS;
    }
    
    // Refit when crossing power-of-10 thresholds
    uint32_t last_count = state->last_refit_sample_count;
    
    // Check for power-of-10 crossings: 10, 100, 1000, etc.
    uint32_t threshold = 10;
    while (threshold < current_sample_count) {
        if (last_count < threshold && current_sample_count >= threshold) {
            return true;
        }
        threshold *= 10;
    }
    
    return false;
}

error_code_t hoge_polynomial_get_coefficients(const poly_fit_state_t* state,
                                               float coefficients_output[HOGE_NUM_COEFFICIENTS]) {
    if (!state || !coefficients_output) {
        return ERR_INVALID_ARG;
    }
    
    if (!state->polynomial.valid) {
        return ERR_NOT_INITIALIZED;
    }
    
    for (int i = 0; i < HOGE_NUM_COEFFICIENTS; i++) {
        coefficients_output[i] = state->polynomial.A[i];
    }
    
    return ERR_OK;
}

void hoge_polynomial_reset(poly_fit_state_t* state) {
    if (!state) {
        return;
    }
    
    state->sample_count = 0;
    state->polynomial.valid = false;
    state->needs_refit = false;
    state->last_refit_sample_count = 0;
    
    // Clear samples array if it exists
    if (state->samples) {
        memset(state->samples, 0, state->max_samples * sizeof(poly_calibration_sample_t));
    }
}

bool hoge_polynomial_check_range(const poly_fit_state_t* state,
                                  float resistance_ohm,
                                  float* min_resistance_output,
                                  float* max_resistance_output) {
    if (!state || state->sample_count == 0) {
        return false;
    }
    
    // Find min and max resistance from samples
    float min_ln_r = state->samples[0].ln_resistance;
    float max_ln_r = state->samples[0].ln_resistance;
    
    for (uint32_t i = 1; i < state->sample_count; i++) {
        if (state->samples[i].ln_resistance < min_ln_r) {
            min_ln_r = state->samples[i].ln_resistance;
        }
        if (state->samples[i].ln_resistance > max_ln_r) {
            max_ln_r = state->samples[i].ln_resistance;
        }
    }
    
    if (min_resistance_output) {
        *min_resistance_output = expf(min_ln_r);
    }
    if (max_resistance_output) {
        *max_resistance_output = expf(max_ln_r);
    }
    
    return (resistance_ohm >= expf(min_ln_r) && resistance_ohm <= expf(max_ln_r));
}

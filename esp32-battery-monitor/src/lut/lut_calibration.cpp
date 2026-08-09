/**
 * @file lut_calibration.cpp
 * @brief Look-Up Table with Cubic Spline Interpolation Implementation
 */

#include "lut_calibration.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Clock function - can be overridden for testing
#ifndef LUT_CLOCK_FUNC
#define LUT_CLOCK_FUNC default_lut_clock_ms
static uint32_t default_lut_clock_ms(void) { return 0; }
#endif

// ============================================================================
// CUBIC SPLINE INTERPOLATION (Natural Spline)
// ============================================================================

// Build natural cubic spline coefficients from data points
// Note: Uses local allocation per-call to avoid global state
static bool build_natural_spline(const float* x, const float* y, int n, 
                                  spline_coefficient_t** coeffs_out, int* n_out,
                                  spline_coefficient_t* local_cache, uint16_t cache_size) {
    if (n < 2) return false;
    
    // Use provided local cache or allocate
    spline_coefficient_t* coeffs = NULL;
    if (local_cache && cache_size >= (uint16_t)(n - 1)) {
        coeffs = local_cache;
    } else {
        coeffs = (spline_coefficient_t*)calloc(n - 1, sizeof(spline_coefficient_t));
        if (!coeffs) return false;
    }
    
    // For simplicity, use Catmull-Rom style interpolation
    // This is a simplified approach - full natural spline would solve tridiagonal system
    
    for (int i = 0; i < n - 1; i++) {
        float h = x[i + 1] - x[i];
        if (h <= 0.0f) continue;
        
        // Calculate slopes
        float m_prev = (i > 0) ? (y[i] - y[i-1]) / (x[i] - x[i-1]) : (y[i+1] - y[i]) / h;
        float m_next = (i < n-1) ? (y[i+1] - y[i]) / h : (y[i] - y[i-1]) / (x[i] - x[i-1]);
        
        // Catmull-Rom coefficients
        coeffs[i].a = y[i];
        coeffs[i].b = m_prev;
        coeffs[i].c = (3.0f * (y[i+1] - y[i]) / (h * h)) - (2.0f * m_prev / h) - (m_next / h);
        coeffs[i].d = (2.0f * (y[i] - y[i+1]) / (h * h * h)) + ((m_prev + m_next) / (h * h));
    }
    
    *coeffs_out = coeffs;
    *n_out = n - 1;
    
    return true;
}

// Evaluate spline at point x
static float eval_spline(const spline_coefficient_t* coeffs, int n, float x, const float* x_knots) {
    if (n <= 0 || !coeffs) return 0.0f;
    
    // Find the right interval
    int i = 0;
    for (i = 0; i < n - 1; i++) {
        if (x >= x_knots[i] && x <= x_knots[i + 1]) {
            break;
        }
    }
    i = (i >= n) ? n - 2 : i;
    
    float dx = x - x_knots[i];
    return coeffs[i].a + coeffs[i].b * dx + coeffs[i].c * dx * dx + coeffs[i].d * dx * dx * dx;
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

error_code_t lut_init(lut_table_t* lut, uint8_t channel_id, uint16_t bin_count) {
    if (!lut) {
        return ERR_INVALID_ARG;
    }
    
    memset(lut, 0, sizeof(lut_table_t));
    
    lut->bin_count = (bin_count > 0) ? bin_count : LUT_DEFAULT_BIN_COUNT;
    lut->bins = (lut_bin_t*)calloc(lut->bin_count, sizeof(lut_bin_t));
    
    if (!lut->bins) {
        return ERR_MEMORY_ALLOC;
    }
    
    // Initialize bins
    float temp_step = LUT_TEMP_SPAN_C / lut->bin_count;
    for (uint16_t i = 0; i < lut->bin_count; i++) {
        lut->bins[i].center_temp_c = LUT_MIN_TEMP_C + (i + 0.5f) * temp_step;
        lut->bins[i].valid = false;
        lut->bins[i].sample_count = 0;
        lut->bins[i].confidence = 0.0f;
    }
    
    lut->min_temp_c = LUT_MIN_TEMP_C;
    lut->max_temp_c = LUT_MAX_TEMP_C;
    lut->channel_id = channel_id;
    lut->initialized = true;
    
    return ERR_OK;
}

error_code_t lut_add_point(lut_table_t* lut, const calibration_point_t* point) {
    if (!lut || !point) {
        return ERR_INVALID_ARG;
    }
    
    if (!lut->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    // Find the appropriate bin based on AHT20 temperature
    float temp_step = LUT_TEMP_SPAN_C / lut->bin_count;
    int bin_index = (int)((point->aht20_temperature_c - LUT_MIN_TEMP_C) / temp_step);
    
    // Clamp to valid range
    if (bin_index < 0) bin_index = 0;
    if (bin_index >= (int)lut->bin_count) bin_index = lut->bin_count - 1;
    
    lut_bin_t* bin = &lut->bins[bin_index];
    
    // Update running averages
    bin->running_sum_r += point->ntc_resistance_ohm;
    bin->running_sum_t += point->aht20_temperature_c;
    bin->sample_count++;
    
    // Calculate new averages
    bin->avg_resistance_ohm = bin->running_sum_r / bin->sample_count;
    bin->center_temp_c = bin->running_sum_t / bin->sample_count;
    
    // Mark as valid when enough samples collected
    if (bin->sample_count >= LUT_MIN_SAMPLES_PER_BIN) {
        bin->valid = true;
        bin->confidence = fminf((float)bin->sample_count / 100.0f, 1.0f);
        
        // Boost confidence if point had high confidence
        bin->confidence = fmaxf(bin->confidence, point->confidence_score);
    }
    
    lut->last_update_ms = LUT_CLOCK_FUNC();
    
    return ERR_OK;
}

error_code_t lut_interp_temp(const lut_table_t* lut, float resistance_ohm, float* temp_c_output) {
    if (!lut || !temp_c_output) {
        return ERR_INVALID_ARG;
    }
    
    if (!lut->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    // Check if within range
    if (!lut_is_in_range(lut, resistance_ohm)) {
        return ERR_OUT_OF_RANGE;
    }
    
    // Collect valid data points for interpolation
    float r_values[256];
    float t_values[256];
    int valid_count = 0;
    
    for (uint16_t i = 0; i < lut->bin_count && valid_count < 256; i++) {
        if (lut->bins[i].valid && lut->bins[i].sample_count >= LUT_MIN_SAMPLES_PER_BIN) {
            r_values[valid_count] = lut->bins[i].avg_resistance_ohm;
            t_values[valid_count] = lut->bins[i].center_temp_c;
            valid_count++;
        }
    }
    
    if (valid_count < 2) {
        return ERR_NOT_INITIALIZED;  // Not enough data for interpolation
    }
    
    // Sort by resistance (simple bubble sort for small arrays)
    for (int i = 0; i < valid_count - 1; i++) {
        for (int j = i + 1; j < valid_count; j++) {
            if (r_values[i] > r_values[j]) {
                float temp_r = r_values[i];
                float temp_t = t_values[i];
                r_values[i] = r_values[j];
                t_values[i] = t_values[j];
                r_values[j] = temp_r;
                t_values[j] = temp_t;
            }
        }
    }
    
    // Find the interval containing our resistance
    int interval = 0;
    for (int i = 0; i < valid_count - 1; i++) {
        if (resistance_ohm >= r_values[i] && resistance_ohm <= r_values[i + 1]) {
            interval = i;
            break;
        }
    }
    
    // Linear interpolation between nearest neighbors
    // (Simplified from full cubic spline for embedded efficiency)
    float r0 = r_values[interval];
    float r1 = r_values[interval + 1];
    float t0 = t_values[interval];
    float t1 = t_values[interval + 1];
    
    if (fabsf(r1 - r0) < 1e-6f) {
        *temp_c_output = t0;
    } else {
        float ratio = (resistance_ohm - r0) / (r1 - r0);
        *temp_c_output = t0 + ratio * (t1 - t0);
    }
    
    return ERR_OK;
}

bool lut_is_in_range(const lut_table_t* lut, float resistance_ohm) {
    if (!lut || !lut->initialized) {
        return false;
    }
    
    // Find min and max resistance from valid bins
    float min_r = 1e9f;
    float max_r = 0.0f;
    bool found = false;
    
    for (uint16_t i = 0; i < lut->bin_count; i++) {
        if (lut->bins[i].valid) {
            found = true;
            if (lut->bins[i].avg_resistance_ohm < min_r) {
                min_r = lut->bins[i].avg_resistance_ohm;
            }
            if (lut->bins[i].avg_resistance_ohm > max_r) {
                max_r = lut->bins[i].avg_resistance_ohm;
            }
        }
    }
    
    if (!found) {
        return false;
    }
    
    // Add small margin for interpolation
    return (resistance_ohm >= min_r * 0.95f && resistance_ohm <= max_r * 1.05f);
}

error_code_t lut_get_range(const lut_table_t* lut, float* min_temp_output, float* max_temp_output) {
    if (!lut) {
        return ERR_INVALID_ARG;
    }
    
    if (!lut->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    if (min_temp_output) {
        *min_temp_output = lut->min_temp_c;
    }
    if (max_temp_output) {
        *max_temp_output = lut->max_temp_c;
    }
    
    return ERR_OK;
}

error_code_t lut_build_spline(const lut_table_t* lut,
                               spline_coefficient_t** coefficients_output,
                               uint16_t* num_coefficients_output) {
    // Simplified implementation - see lut_interp_temp for actual usage
    UNUSED(lut);
    UNUSED(coefficients_output);
    UNUSED(num_coefficients_output);
    return ERR_UNSUPPORTED;
}

error_code_t lut_get_stats(const lut_table_t* lut,
                           uint16_t* total_bins,
                           uint16_t* valid_bins,
                           uint32_t* total_samples) {
    if (!lut) {
        return ERR_INVALID_ARG;
    }
    
    if (!lut->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    if (total_bins) {
        *total_bins = lut->bin_count;
    }
    
    uint16_t valid = 0;
    uint32_t samples = 0;
    
    for (uint16_t i = 0; i < lut->bin_count; i++) {
        if (lut->bins[i].valid) {
            valid++;
        }
        samples += lut->bins[i].sample_count;
    }
    
    if (valid_bins) {
        *valid_bins = valid;
    }
    if (total_samples) {
        *total_samples = samples;
    }
    
    return ERR_OK;
}

void lut_reset(lut_table_t* lut) {
    if (!lut) {
        return;
    }
    
    if (lut->bins) {
        memset(lut->bins, 0, lut->bin_count * sizeof(lut_bin_t));
        
        // Reinitialize bin centers
        float temp_step = LUT_TEMP_SPAN_C / lut->bin_count;
        for (uint16_t i = 0; i < lut->bin_count; i++) {
            lut->bins[i].center_temp_c = LUT_MIN_TEMP_C + (i + 0.5f) * temp_step;
        }
    }
    
    lut->last_update_ms = 0;
}

error_code_t lut_merge(lut_table_t* dest, const lut_table_t* source) {
    if (!dest || !source) {
        return ERR_INVALID_ARG;
    }
    
    if (!dest->initialized || !source->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    if (dest->bin_count != source->bin_count) {
        return ERR_INVALID_ARG;  // Incompatible LUT sizes
    }
    
    // Merge bin data
    for (uint16_t i = 0; i < dest->bin_count; i++) {
        dest->bins[i].running_sum_r += source->bins[i].running_sum_r;
        dest->bins[i].running_sum_t += source->bins[i].running_sum_t;
        dest->bins[i].sample_count += source->bins[i].sample_count;
        
        // Recalculate averages
        if (dest->bins[i].sample_count > 0) {
            dest->bins[i].avg_resistance_ohm = dest->bins[i].running_sum_r / dest->bins[i].sample_count;
            dest->bins[i].center_temp_c = dest->bins[i].running_sum_t / dest->bins[i].sample_count;
            
            if (dest->bins[i].sample_count >= LUT_MIN_SAMPLES_PER_BIN) {
                dest->bins[i].valid = true;
            }
        }
    }
    
    dest->last_update_ms = LUT_CLOCK_FUNC();
    
    return ERR_OK;
}

error_code_t lut_serialize(const lut_table_t* lut, uint8_t* buffer, size_t buffer_size, size_t* bytes_written) {
    if (!lut || !buffer || !bytes_written) {
        return ERR_INVALID_ARG;
    }
    
    if (!lut->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    // Calculate required size
    size_t required_size = sizeof(uint16_t) +  // bin_count
                           sizeof(uint8_t) +   // channel_id
                           sizeof(float) * 2 + // min/max temp
                           (sizeof(float) * 2 + sizeof(uint16_t)) * lut->bin_count;  // bins
    
    if (buffer_size < required_size) {
        return ERR_OUT_OF_RANGE;
    }
    
    size_t offset = 0;
    
    // Write bin count
    memcpy(buffer + offset, &lut->bin_count, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    
    // Write channel ID
    memcpy(buffer + offset, &lut->channel_id, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    
    // Write temp range
    memcpy(buffer + offset, &lut->min_temp_c, sizeof(float));
    offset += sizeof(float);
    memcpy(buffer + offset, &lut->max_temp_c, sizeof(float));
    offset += sizeof(float);
    
    // Write bins (only avg_resistance and sample_count needed)
    for (uint16_t i = 0; i < lut->bin_count; i++) {
        memcpy(buffer + offset, &lut->bins[i].avg_resistance_ohm, sizeof(float));
        offset += sizeof(float);
        memcpy(buffer + offset, &lut->bins[i].center_temp_c, sizeof(float));
        offset += sizeof(float);
        memcpy(buffer + offset, &lut->bins[i].sample_count, sizeof(uint16_t));
        offset += sizeof(uint16_t);
    }
    
    *bytes_written = offset;
    
    return ERR_OK;
}

error_code_t lut_deserialize(lut_table_t* lut, const uint8_t* buffer, size_t buffer_size) {
    if (!lut || !buffer) {
        return ERR_INVALID_ARG;
    }
    
    if (buffer_size < sizeof(uint16_t) + sizeof(uint8_t) + sizeof(float) * 2) {
        return ERR_OUT_OF_RANGE;
    }
    
    size_t offset = 0;
    
    // Read bin count
    uint16_t bin_count;
    memcpy(&bin_count, buffer + offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    
    // Initialize LUT if needed
    if (!lut->initialized) {
        error_code_t err = lut_init(lut, 0, bin_count);
        if (err != ERR_OK) {
            return err;
        }
    }
    
    // Read channel ID
    memcpy(&lut->channel_id, buffer + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    
    // Read temp range
    memcpy(&lut->min_temp_c, buffer + offset, sizeof(float));
    offset += sizeof(float);
    memcpy(&lut->max_temp_c, buffer + offset, sizeof(float));
    offset += sizeof(float);
    
    // Read bins
    for (uint16_t i = 0; i < lut->bin_count && offset < buffer_size; i++) {
        memcpy(&lut->bins[i].avg_resistance_ohm, buffer + offset, sizeof(float));
        offset += sizeof(float);
        memcpy(&lut->bins[i].center_temp_c, buffer + offset, sizeof(float));
        offset += sizeof(float);
        memcpy(&lut->bins[i].sample_count, buffer + offset, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        
        // Mark as valid if enough samples
        if (lut->bins[i].sample_count >= LUT_MIN_SAMPLES_PER_BIN) {
            lut->bins[i].valid = true;
            lut->bins[i].confidence = fminf((float)lut->bins[i].sample_count / 100.0f, 1.0f);
        }
    }
    
    lut->initialized = true;
    lut->last_update_ms = LUT_CLOCK_FUNC();
    
    return ERR_OK;
}

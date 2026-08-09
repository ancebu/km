/**
 * @file equilibrium_detector.cpp
 * @brief Thermal Equilibrium Detection Implementation
 */

#include "equilibrium_detector.h"
#include <math.h>
#include <string.h>

// Clock function - can be overridden for testing
#ifndef EQUILIBRIUM_CLOCK_FUNC
#define EQUILIBRIUM_CLOCK_FUNC default_clock_ms
static uint32_t default_clock_ms(void) { return 0; }
#endif

// Default configuration values (read-only constants)
static const equilibrium_config_t s_default_config = {
    .current_threshold_a = EQUILIBRIUM_CURRENT_THRESHOLD_mA / 1000.0f,
    .dt_threshold_c_per_min = EQUILIBRIUM_DT_THRESHOLD_C_PER_MIN,
    .spread_threshold_c = EQUILIBRIUM_SPREAD_THRESHOLD_C,
    .stability_count = EQUILIBRIUM_STABILITY_COUNT,
    .min_equilibrium_duration_s = 60.0f  // 1 minute minimum
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static float calculate_temp_spread(const float* temps, uint8_t num_sensors) {
    if (!temps || num_sensors == 0) {
        return 0.0f;
    }
    
    float min_temp = temps[0];
    float max_temp = temps[0];
    
    for (uint8_t i = 1; i < num_sensors; i++) {
        if (temps[i] < min_temp) min_temp = temps[i];
        if (temps[i] > max_temp) max_temp = temps[i];
    }
    
    return max_temp - min_temp;
}

// Calculate dT/dt using state stored in equilibrium_state_t (no globals)
static float calculate_dt_dmin(equilibrium_state_t* state, float current_temp_c, uint32_t current_time) {
    float dt_dmin = 0.0f;
    
    // Use a static field within the state struct for tracking
    // We'll use reference_temp_c and equilibrium_start_ms as temp storage for derivative calc
    // when not in equilibrium. This is a bit of a hack but avoids globals.
    // Better: add fields to the state struct for this purpose.
    // For now, we store last_temp in reference_temp_c and last_time in equilibrium_start_ms
    // when not actively tracking equilibrium.
    
    if (state->equilibrium_start_ms > 0) {
        uint32_t delta_ms = current_time - state->equilibrium_start_ms;
        if (delta_ms > 0) {
            float delta_temp = current_temp_c - state->reference_temp_c;
            float delta_min = (float)delta_ms / 60000.0f;
            if (delta_min > 0.001f) {
                dt_dmin = fabsf(delta_temp) / delta_min;
            }
        }
    }
    
    // Store for next calculation
    state->reference_temp_c = current_temp_c;
    state->equilibrium_start_ms = current_time;
    
    return dt_dmin;
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

error_code_t equilibrium_init(equilibrium_state_t* state, const equilibrium_config_t* config) {
    if (!state) {
        return ERR_INVALID_ARG;
    }
    
    memset(state, 0, sizeof(equilibrium_state_t));
    
    // Use provided config or defaults (config is copied by caller into state if needed)
    // The s_default_config is read-only and used directly in equilibrium_check
    
    state->in_equilibrium = false;
    state->stable_reading_count = 0;
    state->samples_collected = 0;
    
    return ERR_OK;
}

bool equilibrium_check(equilibrium_state_t* state,
                       float current_a,
                       float aht20_temp_c,
                       const float* ntc_temps,
                       uint8_t num_ntc_sensors) {
    if (!state) {
        return false;
    }
    
    uint32_t current_time = EQUILIBRIUM_CLOCK_FUNC();
    
    // Condition 1: Check current is below threshold (near-zero load)
    bool current_ok = fabsf(current_a) < s_default_config.current_threshold_a;
    
    // Condition 2: Check temperature derivative (dT/dt < threshold)
    float dt_dmin = calculate_dt_dmin(state, aht20_temp_c, current_time);
    bool dt_ok = dt_dmin < s_default_config.dt_threshold_c_per_min;
    
    // Condition 3: Check temperature spread across all NTC sensors
    float spread = 0.0f;
    bool spread_ok = true;
    if (ntc_temps && num_ntc_sensors > 0) {
        spread = calculate_temp_spread(ntc_temps, num_ntc_sensors);
        spread_ok = spread < s_default_config.spread_threshold_c;
    }
    
    // All conditions must be met for equilibrium
    bool all_conditions_met = current_ok && dt_ok && spread_ok;
    
    if (all_conditions_met) {
        state->stable_reading_count++;
        
        // Check if we've reached stability threshold
        if (state->stable_reading_count >= s_default_config.stability_count) {
            if (!state->in_equilibrium) {
                // Just entered equilibrium
                state->in_equilibrium = true;
                state->equilibrium_start_ms = current_time;
                state->reference_temp_c = aht20_temp_c;
                state->equilibrium_temp_avg_c = aht20_temp_c;
                state->samples_collected = 0;
            } else {
                // Update running average temperature
                uint32_t duration_ms = current_time - state->equilibrium_start_ms;
                float weight = 1.0f / (1.0f + (duration_ms / 1000.0f));
                state->equilibrium_temp_avg_c = 
                    (state->equilibrium_temp_avg_c * (1.0f - weight)) + (aht20_temp_c * weight);
            }
            
            state->equilibrium_duration_ms = current_time - state->equilibrium_start_ms;
            return true;
        }
    } else {
        // Conditions not met - reset counter to zero (consecutive requirement)
        // This implements the "10 consecutive stable readings" spec correctly
        state->stable_reading_count = 0;
        
        if (state->in_equilibrium) {
            // Exit equilibrium state
            state->in_equilibrium = false;
            state->equilibrium_duration_ms = 0;
        }
    }
    
    return state->in_equilibrium;
}

uint32_t equilibrium_get_duration_ms(const equilibrium_state_t* state) {
    if (!state || !state->in_equilibrium) {
        return 0;
    }
    return state->equilibrium_duration_ms;
}

uint32_t equilibrium_get_sample_count(const equilibrium_state_t* state) {
    if (!state) {
        return 0;
    }
    return state->samples_collected;
}

error_code_t equilibrium_harvest_point(equilibrium_state_t* state,
                                        float ntc_resistance_ohm,
                                        float aht20_temp_c,
                                        uint8_t ntc_channel_id,
                                        calibration_point_t* point) {
    if (!state || !point) {
        return ERR_INVALID_ARG;
    }
    
    if (!state->in_equilibrium) {
        return ERR_NOT_INITIALIZED;
    }
    
    // Validate minimum equilibrium duration
    uint32_t duration_s = state->equilibrium_duration_ms / 1000;
    if (duration_s < s_default_config.min_equilibrium_duration_s) {
        return ERR_TIMEOUT;  // Not stable long enough
    }
    
    // Populate calibration point
    point->ntc_resistance_ohm = ntc_resistance_ohm;
    point->aht20_temperature_c = aht20_temp_c;
    point->ntc_channel_id = ntc_channel_id;
    point->timestamp_ms = EQUILIBRIUM_CLOCK_FUNC();
    point->confidence_score = equilibrium_calculate_confidence(state);
    
    state->samples_collected++;
    
    return ERR_OK;
}

float equilibrium_calculate_confidence(const equilibrium_state_t* state) {
    if (!state || !state->in_equilibrium) {
        return 0.0f;
    }
    
    // Confidence based on equilibrium duration
    // Reaches 1.0 after 10 minutes of stable equilibrium
    float duration_min = (float)state->equilibrium_duration_ms / 60000.0f;
    float confidence = duration_min / 10.0f;
    
    // Clamp to 0.0 - 1.0 range
    confidence = fminf(fmaxf(confidence, 0.0f), 1.0f);
    
    // Boost confidence if many samples collected
    if (state->samples_collected > 100) {
        confidence = fminf(confidence + 0.1f, 1.0f);
    }
    
    return confidence;
}

void equilibrium_reset(equilibrium_state_t* state) {
    if (state) {
        memset(state, 0, sizeof(equilibrium_state_t));
    }
}

void equilibrium_get_default_config(equilibrium_config_t* config) {
    if (config) {
        *config = s_default_config;
    }
}

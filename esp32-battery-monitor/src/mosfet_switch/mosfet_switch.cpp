/**
 * @file mosfet_switch.cpp
 * @brief MOSFET Power Switch Implementation for NTC Self-Heating Mitigation
 */

#include "mosfet_switch.h"
#include <Arduino.h>

// Default configuration
static mosfet_config_t g_default_config = {
    .gate_pin = 12,                  // Default gate pin (configurable)
    .active_high = true,             // Active-high gate drive
    .settle_time_us = MOSFET_SETTLE_TIME_US,
    .cooldown_time_ms = MOSFET_COOLDOWN_TIME_MS
};

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

error_code_t mosfet_switch_init(mosfet_switch_state_t* state, const mosfet_config_t* config) {
    if (!state) {
        return ERR_INVALID_ARG;
    }
    
    memset(state, 0, sizeof(mosfet_switch_state_t));
    
    // Use provided config or defaults
    if (config) {
        g_default_config = *config;
    }
    
    state->gate_pin = g_default_config.gate_pin;
    
    // Configure GPIO
    pinMode(state->gate_pin, OUTPUT);
    
    // Start with MOSFET off (NTC unpowered)
    digitalWrite(state->gate_pin, g_default_config.active_high ? LOW : HIGH);
    state->is_on = false;
    state->session_start_ms = millis();
    
    state->initialized = true;
    
    return ERR_OK;
}

error_code_t mosfet_switch_turn_on(mosfet_switch_state_t* state) {
    if (!state) {
        return ERR_INVALID_ARG;
    }
    
    if (!state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    // Check cooldown period
    if (!mosfet_switch_is_ready(state)) {
        return ERR_TIMEOUT;  // Still in cooldown
    }
    
    // Turn on MOSFET
    digitalWrite(state->gate_pin, g_default_config.active_high ? HIGH : LOW);
    state->is_on = true;
    
    uint32_t current_time = millis();
    state->last_on_time_ms = current_time;
    
    // Wait for settling time
    delayMicroseconds(g_default_config.settle_time_us);
    
    return ERR_OK;
}

error_code_t mosfet_switch_turn_off(mosfet_switch_state_t* state) {
    if (!state) {
        return ERR_INVALID_ARG;
    }
    
    if (!state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    if (!state->is_on) {
        return ERR_OK;  // Already off
    }
    
    // Calculate on-time for duty cycle tracking
    uint32_t current_time = millis();
    uint32_t on_duration = current_time - state->last_on_time_ms;
    state->total_on_time_ms += on_duration;
    state->measurement_count++;
    
    // Turn off MOSFET
    digitalWrite(state->gate_pin, g_default_config.active_high ? LOW : HIGH);
    state->is_on = false;
    
    return ERR_OK;
}

bool mosfet_switch_is_ready(const mosfet_switch_state_t* state) {
    if (!state || !state->initialized) {
        return false;
    }
    
    uint32_t current_time = millis();
    
    // Check if we're still in cooldown after last measurement
    if (current_time - state->last_on_time_ms < g_default_config.cooldown_time_ms) {
        return false;
    }
    
    // Check duty cycle over session
    float duty_cycle = mosfet_switch_get_duty_cycle(state);
    if (duty_cycle > MOSFET_MAX_DUTY_CYCLE_PCT) {
        return false;  // Exceeded max duty cycle
    }
    
    return true;
}

error_code_t mosfet_switch_measure(mosfet_switch_state_t* state,
                                    void (*measure_callback)(void*),
                                    void* user_data) {
    if (!state || !measure_callback) {
        return ERR_INVALID_ARG;
    }
    
    // Check if ready
    if (!mosfet_switch_is_ready(state)) {
        return ERR_TIMEOUT;
    }
    
    // Turn on and wait for settling
    error_code_t err = mosfet_switch_turn_on(state);
    if (err != ERR_OK) {
        return err;
    }
    
    // Call measurement function
    measure_callback(user_data);
    
    // Turn off immediately after
    mosfet_switch_turn_off(state);
    
    return ERR_OK;
}

bool mosfet_switch_get_state(const mosfet_switch_state_t* state) {
    if (!state) {
        return false;
    }
    return state->is_on;
}

float mosfet_switch_get_duty_cycle(const mosfet_switch_state_t* state) {
    if (!state) {
        return 0.0f;
    }
    
    uint32_t current_time = millis();
    uint32_t session_duration = current_time - state->session_start_ms;
    
    if (session_duration == 0) {
        return 0.0f;
    }
    
    float duty_cycle = ((float)state->total_on_time_ms / (float)session_duration) * 100.0f;
    
    return fminf(duty_cycle, 100.0f);
}

void mosfet_switch_reset_duty_cycle(mosfet_switch_state_t* state) {
    if (!state) {
        return;
    }
    
    state->total_on_time_ms = 0;
    state->measurement_count = 0;
    state->session_start_ms = millis();
}

void mosfet_switch_get_default_config(mosfet_config_t* config) {
    if (config) {
        *config = g_default_config;
    }
}

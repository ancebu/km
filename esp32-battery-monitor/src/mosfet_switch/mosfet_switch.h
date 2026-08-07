/**
 * @file mosfet_switch.h
 * @brief MOSFET Power Switch for NTC Self-Heating Mitigation
 * 
 * Hardware-based self-heating mitigation using MOSFET to gate power to NTC beads.
 * Only energizes thermistor circuit during active measurement, eliminating self-heating drift.
 * 
 * Per PDF: Sensor self-heating is a systematic error source; hardware gating eliminates it.
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
// MOSFET SWITCH CONFIGURATION
// ============================================================================
#define MOSFET_SETTLE_TIME_US       100     // Time for MOSFET to fully turn on (microseconds)
#define MOSFET_MEASUREMENT_TIME_MS  50      // Typical measurement duration (milliseconds)
#define MOSFET_COOLDOWN_TIME_MS     1000    // Minimum off time between measurements (ms)
#define MOSFET_MAX_DUTY_CYCLE_PCT   10      // Maximum duty cycle to prevent heating

// ============================================================================
// DATA TYPES
// ============================================================================

/**
 * @brief MOSFET switch driver state
 */
typedef struct {
    uint8_t gate_pin;             // GPIO pin controlling MOSFET gate
    bool initialized;             // true if driver is initialized
    bool is_on;                   // Current MOSFET state
    uint32_t last_on_time_ms;     // Last time MOSFET was turned on
    uint32_t total_on_time_ms;    // Cumulative on-time for duty cycle calculation
    uint32_t session_start_ms;    // When current session started
    uint16_t measurement_count;   // Number of measurements in current session
} mosfet_switch_state_t;

/**
 * @brief MOSFET configuration
 */
typedef struct {
    uint8_t gate_pin;             // GPIO pin for gate control
    bool active_high;             // true if gate high = NTC powered
    uint32_t settle_time_us;      // Settling time after turn-on
    uint32_t cooldown_time_ms;    // Minimum off time between measurements
} mosfet_config_t;

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Initialize MOSFET switch driver
 * @param state Pointer to driver state structure
 * @param config Configuration parameters
 * @return ERR_OK on success, error code otherwise
 */
error_code_t mosfet_switch_init(mosfet_switch_state_t* state, const mosfet_config_t* config);

/**
 * @brief Turn on NTC power (energize thermistor circuit)
 * @param state Pointer to driver state structure
 * @return ERR_OK on success, error code otherwise
 * 
 * Call this immediately before taking a measurement.
 * Waits for MOSFET settling time automatically.
 */
error_code_t mosfet_switch_turn_on(mosfet_switch_state_t* state);

/**
 * @brief Turn off NTC power (de-energize thermistor circuit)
 * @param state Pointer to driver state structure
 * @return ERR_OK on success
 * 
 * Call this immediately after completing a measurement.
 */
error_code_t mosfet_switch_turn_off(mosfet_switch_state_t* state);

/**
 * @brief Check if MOSFET can be turned on (cooldown period elapsed)
 * @param state Pointer to driver state structure
 * @return true if ready for measurement
 */
bool mosfet_switch_is_ready(const mosfet_switch_state_t* state);

/**
 * @brief Perform atomic measurement cycle with proper timing
 * @param state Pointer to driver state structure
 * @param measure_callback Function to call when MOSFET is on and settled
 * @param user_data User data passed to callback
 * @return ERR_OK on success, error code otherwise
 * 
 * This is the recommended way to take measurements:
 * 1. Checks if ready (cooldown elapsed)
 * 2. Turns on MOSFET
 * 3. Waits for settling time
 * 4. Calls your measurement function
 * 5. Turns off MOSFET
 */
error_code_t mosfet_switch_measure(mosfet_switch_state_t* state,
                                    void (*measure_callback)(void*),
                                    void* user_data);

/**
 * @brief Get current MOSFET state
 * @param state Pointer to driver state structure
 * @return true if MOSFET is currently on
 */
bool mosfet_switch_get_state(const mosfet_switch_state_t* state);

/**
 * @brief Calculate duty cycle percentage over recent session
 * @param state Pointer to driver state structure
 * @return Duty cycle as percentage (0-100)
 */
float mosfet_switch_get_duty_cycle(const mosfet_switch_state_t* state);

/**
 * @brief Reset duty cycle tracking
 * @param state Pointer to driver state structure
 */
void mosfet_switch_reset_duty_cycle(mosfet_switch_state_t* state);

/**
 * @brief Get default MOSFET configuration
 * @param config Output: default configuration
 */
void mosfet_switch_get_default_config(mosfet_config_t* config);

#ifdef __cplusplus
}
#endif

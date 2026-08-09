/**
 * @file thermistor_calc.h
 * @brief Thermistor temperature calculation utilities
 * 
 * Converts resistance measurements to temperature using the Beta parameter equation.
 */

#pragma once

#include "types.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calculate temperature from resistance using Beta equation
 * @param resistance_ohm Measured resistance in ohms
 * @param calibration Thermistor calibration data
 * @param valid_output Optional: set to true if result is valid, false on error
 * @return Temperature in Celsius, or NAN on error
 */
float thermistor_resistance_to_temp(float resistance_ohm,
                                     const thermistor_calibration_t* calibration,
                                     bool* valid_output);

/**
 * @brief Calculate resistance from temperature (inverse Beta equation)
 * @param temperature_c Temperature in Celsius
 * @param calibration Thermistor calibration data
 * @return Resistance in ohms, or 0.0f on error
 */
float thermistor_temp_to_resistance(float temperature_c,
                                     const thermistor_calibration_t* calibration);

/**
 * @brief Calculate thermistor resistance from voltage divider measurement
 * @param v_measured Voltage at divider midpoint (V)
 * @param v_excitation Excitation voltage (V)
 * @param series_r Series resistor value (Ω)
 * @return Thermistor resistance in ohms
 */
float thermistor_voltage_to_resistance(float v_measured,
                                        float v_excitation,
                                        float series_r);

/**
 * @brief Validate temperature reading is within reasonable range
 * @param temperature_c Temperature to validate
 * @return true if valid
 */
bool thermistor_is_valid_temp(float temperature_c);

/**
 * @brief Apply calibration offset and gain to temperature reading
 * @param temperature_c Raw temperature
 * @param calibration Calibration data with offset and gain
 * @return Calibrated temperature
 */
float thermistor_apply_calibration(float temperature_c,
                                    const thermistor_calibration_t* calibration);

/**
 * @brief Get default thermistor calibration from config
 * @param calibration Output: calibration data
 */
void thermistor_get_default_calibration(thermistor_calibration_t* calibration);

#ifdef __cplusplus
}
#endif

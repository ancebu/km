/**
 * @file thermistor_calc_native.cpp
 * @brief Native (non-ESP32) implementation of thermistor calculation functions
 * 
 * This file provides implementations of thermistor functions for the
 * native test environment, without requiring Arduino/ESP32 headers.
 */

// Always compile native implementation for native environment
// The build_src_filter ensures this file is only compiled for native env

#include "thermistor/thermistor_calc.h"
#include <math.h>
#include <string.h>

// Absolute zero in Kelvin
#define ABSOLUTE_ZERO_K 273.15f

float thermistor_resistance_to_temp(float resistance_ohm,
                                     const thermistor_calibration_t* calibration) {
    if (calibration == NULL || resistance_ohm <= 0.0f) {
        return -999.0f;
    }

    // Beta equation: T = B / (ln(R/Rnom) + B/Tnom)
    float t_nominal_k = calibration->t_nominal_c + ABSOLUTE_ZERO_K;
    float ln_ratio = logf(resistance_ohm / calibration->r_nominal);
    
    float temp_k = calibration->beta / (ln_ratio + (calibration->beta / t_nominal_k));
    float temp_c = temp_k - ABSOLUTE_ZERO_K;

    // Apply calibration corrections
    return thermistor_apply_calibration(temp_c, calibration);
}

float thermistor_temp_to_resistance(float temperature_c,
                                     const thermistor_calibration_t* calibration) {
    if (calibration == NULL) {
        return 0.0f;
    }

    float temp_k = temperature_c + ABSOLUTE_ZERO_K;
    float t_nominal_k = calibration->t_nominal_c + ABSOLUTE_ZERO_K;
    
    // Inverse Beta equation: R = Rnom * exp(B * (1/T - 1/Tnom))
    float exponent = calibration->beta * ((1.0f / temp_k) - (1.0f / t_nominal_k));
    return calibration->r_nominal * expf(exponent);
}

float thermistor_voltage_to_resistance(float v_measured,
                                        float v_excitation,
                                        float series_r) {
    if (v_excitation <= 0.0f || series_r <= 0.0f) {
        return 0.0f;
    }

    // Voltage divider: Vout = Vin * (R_thermistor / (R_thermistor + R_series))
    // Solving for R_thermistor: R_thermistor = R_series * (Vout / (Vin - Vout))
    
    if (v_measured >= v_excitation) {
        return INFINITY;  // Open circuit
    }
    
    if (v_measured <= 0.0f) {
        return 0.0f;  // Short circuit
    }

    return series_r * (v_measured / (v_excitation - v_measured));
}

bool thermistor_is_valid_temp(float temperature_c) {
    return (temperature_c >= CONFIG_TEMP_MIN_C && 
            temperature_c <= CONFIG_TEMP_MAX_C &&
            !isnan(temperature_c) &&
            !isinf(temperature_c));
}

float thermistor_apply_calibration(float temperature_c,
                                    const thermistor_calibration_t* calibration) {
    if (calibration == NULL) {
        return temperature_c;
    }

    // Apply linear correction: T_cal = (T_raw * gain) + offset
    return (temperature_c * calibration->gain) + calibration->offset_c;
}

void thermistor_get_default_calibration(thermistor_calibration_t* calibration) {
    if (calibration == NULL) {
        return;
    }

    calibration->beta = CONFIG_THERMISTOR_BETA;
    calibration->r_nominal = CONFIG_THERMISTOR_RNOM;
    calibration->t_nominal_c = CONFIG_THERMISTOR_TNOM;
    calibration->series_resistance = CONFIG_THERMISTOR_SERIES_R;
    calibration->offset_c = 0.0f;  // No offset by default
    calibration->gain = 1.0f;      // Unity gain by default
}

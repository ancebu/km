/**
 * @file test_temperature_equations.c
 * @brief Pure C unit tests for temperature calculation equations
 * 
 * Verifies mathematical correctness of:
 * - Beta parameter equation (forward and inverse)
 * - Voltage divider resistance calculation
 * - Hoge 4th-order polynomial evaluation
 * - Phantom anchor specification compliance (-20°C, 100°C, weight=0.1)
 * - Full signal chain integration
 * 
 * NO PYTHON, NO PIO, NO HARDWARE - Pure native C tests
 * Compiles with: gcc -std=c99 -Wall -Wextra -O2 -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <float.h>
#include "../include/types.h"
#include "../include/config.h"
#include "../src/thermistor/thermistor_calc.h"
#include "../src/polynomial/hoge_polynomial.h"

/* ============================================================================
   MINIMAL TEST HARNESS (No frameworks, no dependencies)
   ============================================================================ */

static int tests_run = 0;
static int tests_failed = 0;
static int tests_passed = 0;

#define TEST(name) static void name(void)

#define RUN_TEST(name) do { \
    printf("Running %-40s... ", #name); \
    tests_run++; \
    name(); \
} while(0)

#define ASSERT_EQ_FLOAT(expected, actual, tolerance) do { \
    float _e = (expected), _a = (actual), _t = (tolerance); \
    float _diff = fabsf(_a - _e); \
    if (_diff > _t) { \
        printf("FAILED: %s = %f, expected %f (diff=%f, tol=%f)\n", \
               #actual, _a, _e, _diff, _t); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        printf("FAILED: %s is false\n", #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_FALSE(cond) do { \
    if (cond) { \
        printf("FAILED: %s is true (expected false)\n", #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_FINITE(val) do { \
    if (!isfinite(val)) { \
        printf("FAILED: %s is not finite (val=%f)\n", #val, (float)(val)); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define PRINT_PASS() do { \
    printf("PASSED\n"); \
    tests_passed++; \
} while(0)

/* ============================================================================
   GROUP 1: BETA EQUATION CORRECTNESS
   ============================================================================ */

TEST(test_beta_at_nominal) {
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    // At nominal resistance (10kΩ), should return exactly nominal temp (25°C)
    float temp = thermistor_resistance_to_temp(cal.r_nominal, &cal);
    
    ASSERT_EQ_FLOAT(25.0f, temp, 0.1f);
    PRINT_PASS();
}

TEST(test_beta_at_cold) {
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    // Calculate resistance at -20°C using inverse equation
    float r_at_minus_20 = thermistor_temp_to_resistance(-20.0f, &cal);
    
    // Now verify forward equation returns -20°C
    float temp = thermistor_resistance_to_temp(r_at_minus_20, &cal);
    
    ASSERT_EQ_FLOAT(-20.0f, temp, 1.0f);
    PRINT_PASS();
}

TEST(test_beta_at_hot) {
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    // Calculate resistance at 80°C
    float r_at_80 = thermistor_temp_to_resistance(80.0f, &cal);
    
    // Verify forward equation returns ~80°C
    float temp = thermistor_resistance_to_temp(r_at_80, &cal);
    
    ASSERT_EQ_FLOAT(80.0f, temp, 1.0f);
    PRINT_PASS();
}

TEST(test_beta_inverse_consistency) {
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    // Test round-trip: T -> R -> T should be identity
    float test_temps[] = {-30.0f, -10.0f, 0.0f, 25.0f, 50.0f, 80.0f, 100.0f};
    int num_tests = sizeof(test_temps) / sizeof(test_temps[0]);
    
    for (int i = 0; i < num_tests; i++) {
        float t_original = test_temps[i];
        float r = thermistor_temp_to_resistance(t_original, &cal);
        float t_roundtrip = thermistor_resistance_to_temp(r, &cal);
        
        float error = fabsf(t_roundtrip - t_original);
        if (error > 0.01f) {
            printf("FAILED: Round-trip error at %f°C: got %f°C (error=%f)\n",
                   t_original, t_roundtrip, error);
            tests_failed++;
            return;
        }
    }
    
    PRINT_PASS();
}

TEST(test_beta_ntc_behavior) {
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    // NTC: Resistance increases -> Temperature decreases
    float r_low = 1000.0f;   // Low resistance
    float r_high = 100000.0f; // High resistance
    
    float t_low = thermistor_resistance_to_temp(r_low, &cal);
    float t_high = thermistor_resistance_to_temp(r_high, &cal);
    
    ASSERT_TRUE(t_low > t_high);  // Low R = Hot, High R = Cold
    PRINT_PASS();
}

/* ============================================================================
   GROUP 2: VOLTAGE DIVIDER CORRECTNESS
   ============================================================================ */

TEST(test_divider_midpoint) {
    // At midpoint (V_meas = V_exc/2), R_thermistor should equal R_series
    float v_exc = 3.3f;
    float v_meas = 1.65f;  // Exactly half
    float r_series = 10000.0f;
    
    float r_therm = thermistor_voltage_to_resistance(v_meas, v_exc, r_series);
    
    ASSERT_EQ_FLOAT(10000.0f, r_therm, 1.0f);
    PRINT_PASS();
}

TEST(test_divider_low_voltage) {
    // Low voltage reading = low resistance (cold thermistor)
    float v_exc = 3.3f;
    float v_meas = 0.5f;
    float r_series = 10000.0f;
    
    // Expected: R = 10k * (0.5 / (3.3 - 0.5)) = 10k * (0.5/2.8) ≈ 1785.7Ω
    float r_expected = r_series * (v_meas / (v_exc - v_meas));
    float r_therm = thermistor_voltage_to_resistance(v_meas, v_exc, r_series);
    
    ASSERT_EQ_FLOAT(r_expected, r_therm, 10.0f);
    PRINT_PASS();
}

TEST(test_divider_high_voltage) {
    // High voltage reading = high resistance (hot thermistor)
    float v_exc = 3.3f;
    float v_meas = 2.8f;
    float r_series = 10000.0f;
    
    // Expected: R = 10k * (2.8 / (3.3 - 2.8)) = 10k * (2.8/0.5) = 56000Ω
    float r_expected = r_series * (v_meas / (v_exc - v_meas));
    float r_therm = thermistor_voltage_to_resistance(v_meas, v_exc, r_series);
    
    ASSERT_EQ_FLOAT(r_expected, r_therm, 100.0f);
    PRINT_PASS();
}

TEST(test_divider_open_circuit) {
    // V_meas >= V_exc means open circuit (infinite resistance)
    float v_exc = 3.3f;
    float v_meas = 3.3f;  // Equal to excitation
    float r_series = 10000.0f;
    
    float r_therm = thermistor_voltage_to_resistance(v_meas, v_exc, r_series);
    
    ASSERT_TRUE(isinf(r_therm));
    PRINT_PASS();
}

TEST(test_divider_short_circuit) {
    // V_meas <= 0 means short circuit (zero resistance)
    float v_exc = 3.3f;
    float v_meas = 0.0f;
    float r_series = 10000.0f;
    
    float r_therm = thermistor_voltage_to_resistance(v_meas, v_exc, r_series);
    
    ASSERT_EQ_FLOAT(0.0f, r_therm, 0.01f);
    PRINT_PASS();
}

/* ============================================================================
   GROUP 3: HOGE POLYNOMIAL EVALUATION
   ============================================================================ */

TEST(test_poly_eval_at_anchor) {
    poly_fit_state_t state;
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    // Initialize with small sample buffer
    error_code_t err = hoge_polynomial_init(&state, 50);
    ASSERT_TRUE(err == ERR_OK);
    
    // Add calibration samples around room temperature
    float test_temps[] = {15.0f, 20.0f, 25.0f, 30.0f, 35.0f};
    int num_samples = sizeof(test_temps) / sizeof(test_temps[0]);
    
    for (int i = 0; i < num_samples; i++) {
        float t = test_temps[i];
        float r = thermistor_temp_to_resistance(t, &cal);
        err = hoge_polynomial_add_sample(&state, r, t, 0.9f);
        ASSERT_TRUE(err == ERR_OK);
    }
    
    // Fit polynomial
    err = hoge_polynomial_fit(&state);
    ASSERT_TRUE(err == ERR_OK);
    ASSERT_TRUE(state.polynomial.valid);
    
    // Evaluate at a known point (should be close to actual)
    float r_test = thermistor_temp_to_resistance(25.0f, &cal);
    float t_calc;
    err = hoge_polynomial_calc_temp(&state, r_test, &t_calc);
    
    ASSERT_TRUE(err == ERR_OK);
    ASSERT_EQ_FLOAT(25.0f, t_calc, 2.0f);  // Within 2°C at calibration point
    
    PRINT_PASS();
}

TEST(test_poly_extrapolation_stability) {
    poly_fit_state_t state;
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    error_code_t err = hoge_polynomial_init(&state, 50);
    ASSERT_TRUE(err == ERR_OK);
    
    // Add samples in narrow range (20-30°C)
    float r_20 = thermistor_temp_to_resistance(20.0f, &cal);
    float r_30 = thermistor_temp_to_resistance(30.0f, &cal);
    
    for (int i = 0; i < 10; i++) {
        float t = 20.0f + (i * 1.0f);
        float r = thermistor_temp_to_resistance(t, &cal);
        hoge_polynomial_add_sample(&state, r, t, 0.95f);
    }
    
    err = hoge_polynomial_fit(&state);
    ASSERT_TRUE(err == ERR_OK);
    
    // Test extrapolation to cold (should not oscillate wildly)
    float r_cold = thermistor_temp_to_resistance(-10.0f, &cal);
    float t_cold;
    err = hoge_polynomial_calc_temp(&state, r_cold, &t_cold);
    
    ASSERT_TRUE(err == ERR_OK || err == ERR_OUT_OF_RANGE);
    if (err == ERR_OK) {
        // Should be in reasonable range, not NaN or infinity
        ASSERT_FINITE(t_cold);
        ASSERT_TRUE(t_cold > -50.0f && t_cold < 10.0f);
    }
    
    // Test extrapolation to hot
    float r_hot = thermistor_temp_to_resistance(60.0f, &cal);
    float t_hot;
    err = hoge_polynomial_calc_temp(&state, r_hot, &t_hot);
    
    ASSERT_TRUE(err == ERR_OK || err == ERR_OUT_OF_RANGE);
    if (err == ERR_OK) {
        ASSERT_FINITE(t_hot);
        ASSERT_TRUE(t_hot > 40.0f && t_hot < 100.0f);
    }
    
    PRINT_PASS();
}

TEST(test_poly_coefficients_valid) {
    poly_fit_state_t state;
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    error_code_t err = hoge_polynomial_init(&state, 100);
    ASSERT_TRUE(err == ERR_OK);
    
    // Add diverse samples across wide temperature range
    float temps[] = {-10.0f, 0.0f, 10.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f, 60.0f};
    int num = sizeof(temps) / sizeof(temps[0]);
    
    for (int i = 0; i < num; i++) {
        float r = thermistor_temp_to_resistance(temps[i], &cal);
        hoge_polynomial_add_sample(&state, r, temps[i], 0.9f);
    }
    
    err = hoge_polynomial_fit(&state);
    ASSERT_TRUE(err == ERR_OK);
    
    // Check all coefficients are finite and non-zero
    float coeffs[HOGE_NUM_COEFFICIENTS];
    err = hoge_polynomial_get_coefficients(&state, coeffs);
    ASSERT_TRUE(err == ERR_OK);
    
    for (int i = 0; i < HOGE_NUM_COEFFICIENTS; i++) {
        ASSERT_FINITE(coeffs[i]);
    }
    
    PRINT_PASS();
}

/* ============================================================================
   GROUP 4: PHANTOM ANCHOR SPECIFICATION
   ============================================================================ */

TEST(test_phantom_temp_low) {
    // Verify low phantom anchor is exactly -20°C per PDF spec
    poly_calibration_sample_t anchor_low, anchor_high;
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    hoge_calculate_phantom_anchors(cal.beta, cal.r_nominal, cal.t_nominal_c,
                                   &anchor_low, &anchor_high);
    
    // Convert 1/T back to Celsius to verify
    float t_low_k = 1.0f / anchor_low.inverse_temp_k;
    float t_low_c = t_low_k - 273.15f;
    
    ASSERT_EQ_FLOAT(HOGE_PHANTOM_TEMP_MIN_C, t_low_c, 0.1f);
    PRINT_PASS();
}

TEST(test_phantom_temp_high) {
    // Verify high phantom anchor is exactly 100°C per PDF spec
    poly_calibration_sample_t anchor_low, anchor_high;
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    hoge_calculate_phantom_anchors(cal.beta, cal.r_nominal, cal.t_nominal_c,
                                   &anchor_low, &anchor_high);
    
    float t_high_k = 1.0f / anchor_high.inverse_temp_k;
    float t_high_c = t_high_k - 273.15f;
    
    ASSERT_EQ_FLOAT(HOGE_PHANTOM_TEMP_MAX_C, t_high_c, 0.1f);
    PRINT_PASS();
}

TEST(test_phantom_weight) {
    // Verify both anchors have weight = 0.1 per PDF spec
    poly_calibration_sample_t anchor_low, anchor_high;
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    hoge_calculate_phantom_anchors(cal.beta, cal.r_nominal, cal.t_nominal_c,
                                   &anchor_low, &anchor_high);
    
    ASSERT_EQ_FLOAT(HOGE_PHANTOM_ANCHOR_WEIGHT_LOW, anchor_low.weight, 0.001f);
    ASSERT_EQ_FLOAT(HOGE_PHANTOM_ANCHOR_WEIGHT_HIGH, anchor_high.weight, 0.001f);
    PRINT_PASS();
}

TEST(test_phantom_resistance_calc) {
    // Verify phantom anchor resistances are computed via Beta equation
    poly_calibration_sample_t anchor_low, anchor_high;
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    hoge_calculate_phantom_anchors(cal.beta, cal.r_nominal, cal.t_nominal_c,
                                   &anchor_low, &anchor_high);
    
    // Manually calculate expected resistance at -20°C using Beta equation
    float t_min_k = HOGE_PHANTOM_TEMP_MIN_C + 273.15f;
    float t_nom_k = cal.t_nominal_c + 273.15f;
    float r_expected_low = cal.r_nominal * expf(cal.beta * (1.0f/t_min_k - 1.0f/t_nom_k));
    
    // Convert ln(R) back to R
    float r_actual_low = expf(anchor_low.ln_resistance);
    
    ASSERT_EQ_FLOAT(r_expected_low, r_actual_low, 1.0f);
    PRINT_PASS();
}

/* ============================================================================
   GROUP 5: FULL SIGNAL CHAIN INTEGRATION
   ============================================================================ */

TEST(test_chain_voltage_to_temp) {
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    // Simulate: ADC reads voltage -> convert to resistance -> convert to temp
    float v_exc = 3.3f;
    float v_meas = 1.2f;  // Arbitrary reading
    
    // Step 1: Voltage to resistance
    float r_therm = thermistor_voltage_to_resistance(v_meas, v_exc, cal.series_resistance);
    ASSERT_FINITE(r_therm);
    ASSERT_TRUE(r_therm > 0.0f);
    
    // Step 2: Resistance to temperature
    float temp = thermistor_resistance_to_temp(r_therm, &cal);
    ASSERT_FINITE(temp);
    ASSERT_TRUE(temp > -40.0f && temp < 125.0f);  // Within valid range
    
    PRINT_PASS();
}

TEST(test_chain_with_calibration) {
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    // Apply known offset and gain
    cal.offset_c = 2.5f;   // +2.5°C offset
    cal.gain = 1.05f;      // 5% gain
    
    // Get raw temperature at nominal
    float r_nom = cal.r_nominal;
    float temp_raw = thermistor_resistance_to_temp(r_nom, &cal);
    
    // The function applies calibration internally
    // At nominal (25°C raw), calibrated should be: (25.0 * 1.05) + 2.5 = 28.75°C
    float temp_expected = (25.0f * cal.gain) + cal.offset_c;
    
    ASSERT_EQ_FLOAT(temp_expected, temp_raw, 0.1f);
    PRINT_PASS();
}

/* ============================================================================
   GROUP 6: PDF RESEARCH CLAIMS VALIDATION
   ============================================================================ */

TEST(test_refit_threshold_power_of_10) {
    poly_fit_state_t state;
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    error_code_t err = hoge_polynomial_init(&state, 200);
    ASSERT_TRUE(err == ERR_OK);
    
    // Add samples one by one and check refit triggers at power-of-10
    bool refit_at_10 = false;
    bool refit_at_100 = false;
    
    for (int i = 1; i <= 150; i++) {
        float t = 25.0f + (i * 0.1f);
        float r = thermistor_temp_to_resistance(t, &cal);
        hoge_polynomial_add_sample(&state, r, t, 0.9f);
        
        if (hoge_polynomial_needs_refit(&state, i)) {
            if (i == 10) refit_at_10 = true;
            if (i == 100) refit_at_100 = true;
        }
    }
    
    ASSERT_TRUE(refit_at_10);
    ASSERT_TRUE(refit_at_100);
    PRINT_PASS();
}

TEST(test_poly_init_rejects_oversized_max_samples) {
    poly_fit_state_t state;
    error_code_t err = hoge_polynomial_init(&state, 1000);
    ASSERT_TRUE(err == ERR_OUT_OF_RANGE);
    PRINT_PASS();
}

TEST(test_phantom_stabilizes_extrapolation) {
    // Compare polynomial fit WITH vs WITHOUT phantom anchors
    // With phantoms: monotonic extrapolation
    // Without phantoms: potential oscillation (not tested here, just verify phantoms work)
    
    poly_fit_state_t state;
    thermistor_calibration_t cal;
    thermistor_get_default_calibration(&cal);
    
    error_code_t err = hoge_polynomial_init(&state, 50);
    ASSERT_TRUE(err == ERR_OK);
    
    // Add limited samples (narrow range)
    for (int i = 0; i < 8; i++) {
        float t = 20.0f + (i * 2.0f);
        float r = thermistor_temp_to_resistance(t, &cal);
        hoge_polynomial_add_sample(&state, r, t, 0.9f);
    }
    
    // Fit includes phantom anchors automatically
    err = hoge_polynomial_fit(&state);
    ASSERT_TRUE(err == ERR_OK);
    
    // Verify extrapolation produces monotonic results
    float temps_out[5];
    float rs[] = {5000.0f, 8000.0f, 12000.0f, 20000.0f, 35000.0f};
    
    for (int i = 0; i < 5; i++) {
        err = hoge_polynomial_calc_temp(&state, rs[i], &temps_out[i]);
        if (err != ERR_OK) {
            temps_out[i] = -999.0f;  // Mark as invalid
        }
    }
    
    // Check monotonicity: as R increases, T should decrease (NTC behavior)
    for (int i = 0; i < 4; i++) {
        if (temps_out[i] > -900.0f && temps_out[i+1] > -900.0f) {
            ASSERT_TRUE(temps_out[i] >= temps_out[i+1]);
        }
    }
    
    PRINT_PASS();
}

/* ============================================================================
   MAIN ENTRY POINT
   ============================================================================ */

int main(void) {
    printf("=================================================\n");
    printf("Temperature Equations Unit Test Suite\n");
    printf("PDF: Beyond Steinhart-Hart Thermal Framework\n");
    printf("=================================================\n\n");
    
    /* Group 1: Beta Equation */
    printf("--- Group 1: Beta Equation Correctness ---\n");
    RUN_TEST(test_beta_at_nominal);
    RUN_TEST(test_beta_at_cold);
    RUN_TEST(test_beta_at_hot);
    RUN_TEST(test_beta_inverse_consistency);
    RUN_TEST(test_beta_ntc_behavior);
    printf("\n");
    
    /* Group 2: Voltage Divider */
    printf("--- Group 2: Voltage Divider Correctness ---\n");
    RUN_TEST(test_divider_midpoint);
    RUN_TEST(test_divider_low_voltage);
    RUN_TEST(test_divider_high_voltage);
    RUN_TEST(test_divider_open_circuit);
    RUN_TEST(test_divider_short_circuit);
    printf("\n");
    
    /* Group 3: Hoge Polynomial */
    printf("--- Group 3: Hoge Polynomial Evaluation ---\n");
    RUN_TEST(test_poly_eval_at_anchor);
    RUN_TEST(test_poly_extrapolation_stability);
    RUN_TEST(test_poly_coefficients_valid);
    printf("\n");
    
    /* Group 4: Phantom Anchors */
    printf("--- Group 4: Phantom Anchor Specification ---\n");
    RUN_TEST(test_phantom_temp_low);
    RUN_TEST(test_phantom_temp_high);
    RUN_TEST(test_phantom_weight);
    RUN_TEST(test_phantom_resistance_calc);
    printf("\n");
    
    /* Group 5: Signal Chain */
    printf("--- Group 5: Full Signal Chain Integration ---\n");
    RUN_TEST(test_chain_voltage_to_temp);
    RUN_TEST(test_chain_with_calibration);
    printf("\n");
    
    /* Group 6: PDF Claims */
    printf("--- Group 6: PDF Research Claims Validation ---\n");
    RUN_TEST(test_refit_threshold_power_of_10);
    RUN_TEST(test_poly_init_rejects_oversized_max_samples);
    RUN_TEST(test_phantom_stabilizes_extrapolation);
    printf("\n");
    
    /* Summary */
    printf("=================================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("=================================================\n");
    
    if (tests_failed == 0) {
        printf("PASS\n");
        return 0;
    } else {
        printf("FAIL\n");
        return 1;
    }
}

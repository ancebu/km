/**
 * @file config.h
 * @brief Compile-time configuration for ESP32 Battery Monitor
 * 
 * Centralized configuration file for all compile-time constants.
 * Modify this file to customize pin assignments, sampling rates,
 * and hardware parameters without changing source code.
 */

#pragma once

// ============================================================================
// GPIO_NUM_* FALLBACKS FOR NATIVE BUILDS
// ============================================================================
#ifndef ARDUINO_ARCH_ESP32
#ifndef GPIO_NUM_0
#define GPIO_NUM_0   0
#define GPIO_NUM_1   1
#define GPIO_NUM_2   2
#define GPIO_NUM_3   3
#define GPIO_NUM_4   4
#define GPIO_NUM_5   5
#define GPIO_NUM_12  12
#define GPIO_NUM_18  18
#define GPIO_NUM_19  19
#define GPIO_NUM_21  21
#define GPIO_NUM_22  22
#define GPIO_NUM_23  23
#define GPIO_NUM_25  25
#define GPIO_NUM_26  26
#define GPIO_NUM_27  27
#define GPIO_NUM_32  32
#define GPIO_NUM_33  33
#define GPIO_NUM_34  34
#define GPIO_NUM_35  35
#endif
#endif

// ============================================================================
// LOGGING CONFIGURATION
// ============================================================================
#define CONFIG_LOG_LEVEL_DEBUG      1
#define CONFIG_LOG_LEVEL_INFO       2
#define CONFIG_LOG_LEVEL_WARN       3
#define CONFIG_LOG_LEVEL_ERROR      4
#define CONFIG_LOG_LEVEL_NONE       5

#ifndef CONFIG_LOG_LEVEL
    #define CONFIG_LOG_LEVEL CONFIG_LOG_LEVEL_INFO
#endif

// ============================================================================
// GPIO PIN ASSIGNMENTS (Customize for your board)
// ============================================================================

// I2C Bus Configuration
#define CONFIG_I2C_SDA_PIN        GPIO_NUM_21
#define CONFIG_I2C_SCL_PIN        GPIO_NUM_22
#define CONFIG_I2C_FREQUENCY      400000U  // 400 kHz

// Multiplexer A Control Pins (8-channel example)
#define CONFIG_MUX_A_SELECT_0     GPIO_NUM_18
#define CONFIG_MUX_A_SELECT_1     GPIO_NUM_19
#define CONFIG_MUX_A_SELECT_2     GPIO_NUM_23
#define CONFIG_MUX_A_ENABLE       GPIO_NUM_5  // Active low

// Multiplexer B Control Pins (8-channel example)
#define CONFIG_MUX_B_SELECT_0     GPIO_NUM_25
#define CONFIG_MUX_B_SELECT_1     GPIO_NUM_26
#define CONFIG_MUX_B_SELECT_2     GPIO_NUM_27
#define CONFIG_MUX_B_ENABLE       GPIO_NUM_4  // Active low

// Optional: Third mux for additional channels
#define CONFIG_MUX_C_ENABLED      0
#if CONFIG_MUX_C_ENABLED
    #define CONFIG_MUX_C_SELECT_0   GPIO_NUM_32
    #define CONFIG_MUX_C_SELECT_1   GPIO_NUM_33
    #define CONFIG_MUX_C_SELECT_2   GPIO_NUM_34
    #define CONFIG_MUX_C_ENABLE     GPIO_NUM_35
#endif

// NTC Power Control MOSFET Gate Pin (self-heating mitigation)
#define CONFIG_NTC_MOSFET_GATE_PIN  GPIO_NUM_12
#define CONFIG_NTC_MOSFET_ACTIVE_HIGH 1

// ============================================================================
// MULTIPLEXER CONFIGURATION
// ============================================================================
#define CONFIG_MUX_CHANNELS_A     8    // Number of channels on MUX A
#define CONFIG_MUX_CHANNELS_B     8    // Number of channels on MUX B
#define CONFIG_MUX_SETTLING_MS    10   // Delay after channel switch (ms)

// Total matrix size (A x B)
#define CONFIG_MATRIX_ROWS      CONFIG_MUX_CHANNELS_A
#define CONFIG_MATRIX_COLS      CONFIG_MUX_CHANNELS_B
#define CONFIG_MATRIX_SIZE      (CONFIG_MATRIX_ROWS * CONFIG_MATRIX_COLS)

// ============================================================================
// INA226 CONFIGURATION
// ============================================================================
#define CONFIG_INA226_ADDRESS   0x40   // Default I2C address
#define CONFIG_INA226_SHUNT_R   0.002f // Shunt resistor in ohms (2mΩ)
#define CONFIG_INA226_MAX_CURRENT 10.0f // Maximum expected current (A)

// Averaging settings (trade speed for accuracy)
#define CONFIG_INA226_AVG_SAMPLES 16   // 1, 4, 16, 64, 128, 256, 512, 1024
#define CONFIG_INA226_CONV_TIME   1    // Conversion time: 1=140us, 2=204us, etc.

// ============================================================================
// THERMISTOR CONFIGURATION
// ============================================================================
#define CONFIG_THERMISTOR_BETA    3950.0f  // Beta parameter for NTC
#define CONFIG_THERMISTOR_RNOM    10000.0f // Nominal resistance at Tnom (Ω)
#define CONFIG_THERMISTOR_TNOM    25.0f    // Nominal temperature (°C)
#define CONFIG_THERMISTOR_SERIES_R 10000.0f // Series resistor value (Ω)

// Temperature calculation range
#define CONFIG_TEMP_MIN_C         -40.0f
#define CONFIG_TEMP_MAX_C         125.0f

// Self-heating mitigation
#define CONFIG_SELF_HEATING_MITIGATION_ENABLED  1  // Use MOSFET gating

// ============================================================================
// SELF-CALIBRATION FRAMEWORK CONFIGURATION
// ============================================================================
#define CONFIG_CALIBRATION_ENABLED          1       // Enable opportunistic calibration
#define CONFIG_LUT_BIN_COUNT                256     // LUT bins per NTC channel
#define CONFIG_POLY_MAX_SAMPLES             500     // Max samples for polynomial fit
#define CONFIG_EQUILIBRIUM_MIN_DURATION_S   60      // Minimum stable duration (seconds)

// ============================================================================
// BATTERY CONFIGURATION
// ============================================================================
#define CONFIG_BATTERY_CELLS_SERIES   10   // Number of cells in series
#define CONFIG_BATTERY_CELLS_PARALLEL 4    // Number of cells in parallel
#define CONFIG_CELL_VOLTAGE_MIN       3.0f // Minimum cell voltage (V)
#define CONFIG_CELL_VOLTAGE_MAX       4.2f // Maximum cell voltage (V)
#define CONFIG_PACK_CAPACITY_AH       10.0f // Pack capacity in Ah

// ============================================================================
// SAMPLING CONFIGURATION
// ============================================================================
#define CONFIG_SAMPLE_INTERVAL_MS   1000   // Main loop interval (ms)
#define CONFIG_SCAN_ALL_CHANNELS    1      // Scan entire matrix each cycle
#define CONFIG_CALIBRATE_ON_START   1      // Run calibration at startup

// ============================================================================
// MEMORY & PERFORMANCE
// ============================================================================
#define CONFIG_USE_PSRAM          0      // Use PSRAM if available (ESP32-S3)
#define CONFIG_RING_BUFFER_SIZE   256    // Data logging buffer size
#define CONFIG_NVS_ENABLED        1      // Use NVS for persistent storage

// ============================================================================
// SAFETY & PROTECTION
// ============================================================================
#define CONFIG_OVERTEMP_THRESHOLD_C   60.0f
#define CONFIG_UNDERVOLT_THRESHOLD_V  2.8f
#define CONFIG_OVERVOLT_THRESHOLD_V   4.25f
#define CONFIG_OVERCURRENT_THRESHOLD_A 15.0f

// Enable safety shutdowns
#define CONFIG_SAFETY_ENABLED       1

// ============================================================================
// LCD DISPLAY CONFIGURATION
// ============================================================================
#define CONFIG_LCD_ENABLED          1      // Enable I2C LCD 1602 display
#define CONFIG_LCD_ADDRESS          0      // 0 = auto-detect, or specify 0x27/0x3F
#define CONFIG_LCD_UPDATE_INTERVAL  500    // LCD update interval in ms

// ============================================================================
// DEBUG & TESTING
// ============================================================================
#ifdef DEBUG_MODE
    #undef CONFIG_LOG_LEVEL
    #define CONFIG_LOG_LEVEL CONFIG_LOG_LEVEL_DEBUG
    #define CONFIG_SKIP_CALIBRATION 0  // Always calibrate in debug
#else
    #define CONFIG_SKIP_CALIBRATION 0
#endif

// ============================================================================
// BOARD PROFILES (Override defaults for specific boards)
// ============================================================================
#ifdef BOARD_PROFILE_WROOM_DEVKIT
    // Standard ESP32-WROOM dev board pinout
    #undef CONFIG_I2C_SDA_PIN
    #undef CONFIG_I2C_SCL_PIN
    #define CONFIG_I2C_SDA_PIN GPIO_NUM_21
    #define CONFIG_I2C_SCL_PIN GPIO_NUM_22
#endif

#ifdef BOARD_PROFILE_S3_DEVKIT
    // ESP32-S3 has more GPIO options
    #undef CONFIG_I2C_SDA_PIN
    #undef CONFIG_I2C_SCL_PIN
    #define CONFIG_I2C_SDA_PIN GPIO_NUM_8
    #define CONFIG_I2C_SCL_PIN GPIO_NUM_9
#endif

// Include board-specific overrides if they exist
#if __has_include("board_profile.h")
    #include "board_profile.h"
#endif

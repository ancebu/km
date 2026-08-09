/**
 * @file main.cpp
 * @brief ESP32 Battery Monitor - Main Entry Point
 * 
 * Initializes all subsystems and runs the main monitoring loop.
 */

#include <Arduino.h>
#include <esp_log.h>
#include "config.h"
#include "types.h"
#include "mux/mux_controller.h"
#include "ina226/ina226_driver.h"
#include "thermistor/thermistor_calc.h"
#include "lcd_1602/lcd_1602_i2c.h"

static const char* TAG = "MAIN";

// Global state
static mux_controller_t g_mux;
static ina226_driver_t g_ina226;
static ring_buffer_t g_data_buffer;
static battery_pack_state_t g_pack_state;
static lcd_1602_state_t g_lcd;

/**
 * @brief Initialize all subsystems
 */
static error_code_t initialize_system(void) {
    error_code_t err;

    ESP_LOGI(TAG, "Initializing system...");

    // Initialize data buffer with heap allocation
    err = ring_buffer_init(&g_data_buffer, CONFIG_RING_BUFFER_SIZE);
    if (IS_ERROR(err)) {
        ESP_LOGE(TAG, "Failed to initialize ring buffer: %s", 
                 error_code_to_string(err));
        return err;
    }

    // Initialize multiplexer controller
    err = mux_controller_init(&g_mux, NULL);  // NULL = use default HAL
    if (IS_ERROR(err)) {
        ESP_LOGE(TAG, "Failed to initialize mux controller: %s", 
                 error_code_to_string(err));
        return err;
    }

    // Initialize INA226
    err = ina226_driver_init(&g_ina226, 
                              0,  // I2C port 0
                              CONFIG_I2C_SDA_PIN,
                              CONFIG_I2C_SCL_PIN,
                              CONFIG_INA226_ADDRESS);
    if (IS_ERROR(err)) {
        ESP_LOGE(TAG, "Failed to initialize INA226: %s", 
                 error_code_to_string(err));
        return err;
    }

    // Probe INA226
    if (!ina226_driver_probe(&g_ina226)) {
        ESP_LOGE(TAG, "INA226 not found on I2C bus");
        return ERR_COMMUNICATION;
    }
    ESP_LOGI(TAG, "INA226 found at address 0x%02X", CONFIG_INA226_ADDRESS);

    // Calibrate INA226
#if CONFIG_CALIBRATE_ON_START
    err = ina226_driver_calibrate(&g_ina226, 
                                   CONFIG_INA226_SHUNT_R,
                                   CONFIG_INA226_MAX_CURRENT);
    if (IS_ERROR(err)) {
        ESP_LOGW(TAG, "INA226 calibration failed, using defaults");
    } else {
        ESP_LOGI(TAG, "INA226 calibrated successfully");
    }
#endif

    // Configure INA226 averaging
    err = ina226_driver_configure(&g_ina226,
                                   CONFIG_INA226_AVG_SAMPLES,
                                   CONFIG_INA226_CONV_TIME);
    if (IS_ERROR(err)) {
        ESP_LOGW(TAG, "Failed to configure INA226 averaging");
    }

    // Initialize pack state
    memset(&g_pack_state, 0, sizeof(g_pack_state));
    g_pack_state.num_temp_sensors = CONFIG_MATRIX_SIZE;

    // Initialize LCD display (optional)
#if CONFIG_LCD_ENABLED
    ESP_LOGI(TAG, "Initializing LCD display...");
    err = lcd_1602_init(&g_lcd, 0, NULL);  // 0 = auto-detect address, NULL = default HAL
    if (IS_ERROR(err)) {
        ESP_LOGW(TAG, "LCD initialization failed: %s", error_code_to_string(err));
        ESP_LOGW(TAG, "Continuing without LCD display");
    } else {
        ESP_LOGI(TAG, "LCD initialized successfully");
        lcd_1602_print_at(&g_lcd, 0, 0, "ESP32 Battery");
        lcd_1602_print_at(&g_lcd, 0, 1, "Monitor Ready");
        delay(2000);
        lcd_1602_clear(&g_lcd);
    }
#endif

    ESP_LOGI(TAG, "System initialization complete");
    return ERR_OK;
}

/**
 * @brief Scan thermistor matrix and collect temperature data
 */
static void scan_thermistor_matrix(void) {
    static thermistor_calibration_t therm_cal;
    thermistor_get_default_calibration(&therm_cal);

    ina226_data_t ina_data;
    uint8_t sensor_idx = 0;

    // Enable muxes for scanning
    mux_controller_enable(&g_mux);

    for (uint8_t row = 0; row < CONFIG_MUX_CHANNELS_A; row++) {
        for (uint8_t col = 0; col < CONFIG_MUX_CHANNELS_B; col++) {
            // Set mux channels
            error_code_t err = mux_controller_set_channels(&g_mux, row, col);
            if (IS_ERROR(err)) {
                continue;
            }

            // Read voltage across thermistor
            err = ina226_driver_read(&g_ina226, &ina_data);
            if (IS_ERROR(err)) {
                g_pack_state.temps[sensor_idx].valid = false;
                continue;
            }

            // Calculate resistance from voltage
            float resistance = thermistor_voltage_to_resistance(
                ina_data.voltage_v,
                3.3f,  // V_excitation (adjust for your circuit)
                therm_cal.series_resistance
            );

            // Convert to temperature with validity check
            bool temp_valid = false;
            float temp = thermistor_resistance_to_temp(resistance, &therm_cal, &temp_valid);

            // Store in pack state
            g_pack_state.temps[sensor_idx].temperature_c = temp;
            g_pack_state.temps[sensor_idx].resistance_ohm = resistance;
            g_pack_state.temps[sensor_idx].mux_channel_a = row;
            g_pack_state.temps[sensor_idx].mux_channel_b = col;
            g_pack_state.temps[sensor_idx].valid = temp_valid && thermistor_is_valid_temp(temp);
            g_pack_state.temps[sensor_idx].timestamp_ms = millis();

            sensor_idx++;
        }
    }

    // Disable muxes after scanning
    mux_controller_disable(&g_mux);
}

/**
 * @brief Update pack state with latest measurements
 */
static void update_pack_state(void) {
    ina226_data_t ina_data;
    error_code_t err;

    // Read main pack current/voltage
    err = ina226_driver_read(&g_ina226, &ina_data);
    if (IS_SUCCESS(err)) {
        g_pack_state.pack_voltage_v = ina_data.voltage_v;
        g_pack_state.pack_current_a = ina_data.current_a;
        g_pack_state.pack_power_w = ina_data.power_w;
    }

    // Update timestamp
    g_pack_state.timestamp_ms = millis();

    // Check for faults
    g_pack_state.fault_flags = 0;

#if CONFIG_SAFETY_ENABLED
    // Overvoltage check
    if (g_pack_state.pack_voltage_v > CONFIG_OVERVOLT_THRESHOLD_V) {
        g_pack_state.fault_flags |= FAULT_OVERVOLTAGE;
    }

    // Undervoltage check
    if (g_pack_state.pack_voltage_v < CONFIG_UNDERVOLT_THRESHOLD_V) {
        g_pack_state.fault_flags |= FAULT_UNDERVOLTAGE;
    }

    // Overcurrent check
    if (fabsf(g_pack_state.pack_current_a) > CONFIG_OVERCURRENT_THRESHOLD_A) {
        g_pack_state.fault_flags |= FAULT_OVERCURRENT;
    }

    // Overtemperature check
    for (uint8_t i = 0; i < CONFIG_MATRIX_SIZE; i++) {
        if (g_pack_state.temps[i].valid &&
            g_pack_state.temps[i].temperature_c > CONFIG_OVERTEMP_THRESHOLD_C) {
            g_pack_state.fault_flags |= FAULT_OVERTEMPERATURE;
            break;
        }
    }
#endif

    g_pack_state.safety_fault = (g_pack_state.fault_flags != 0);

#if CONFIG_LCD_ENABLED
    // Update LCD display
    if (g_lcd.initialized) {
        char buffer[17];
        
        // Line 1: Voltage and Current
        snprintf(buffer, sizeof(buffer), "V:%.2fV C:%.2fA", 
                 g_pack_state.pack_voltage_v, g_pack_state.pack_current_a);
        lcd_1602_print_at(&g_lcd, 0, 0, buffer);
        
        // Line 2: Temperature or Power
        if (g_pack_state.num_temp_sensors > 0 && g_pack_state.temps[0].valid) {
            snprintf(buffer, sizeof(buffer), "T:%.1fC P:%.1fW",
                     g_pack_state.temps[0].temperature_c, g_pack_state.pack_power_w);
        } else {
            snprintf(buffer, sizeof(buffer), "P:%.1fW", g_pack_state.pack_power_w);
        }
        lcd_1602_print_at(&g_lcd, 0, 1, buffer);
    }
#endif
}

/**
 * @brief Log current state to serial
 */
static void log_pack_state(void) {
#if CONFIG_LOG_LEVEL <= CONFIG_LOG_LEVEL_INFO
    ESP_LOGI(TAG, "Pack: %.2fV, %.2fA, %.2fW | Faults: 0x%04X",
             g_pack_state.pack_voltage_v,
             g_pack_state.pack_current_a,
             g_pack_state.pack_power_w,
             g_pack_state.fault_flags);

    // Log temperatures (first 4 sensors to avoid spam)
    for (uint8_t i = 0; i < MIN(4, CONFIG_MATRIX_SIZE); i++) {
        if (g_pack_state.temps[i].valid) {
            ESP_LOGD(TAG, "  Temp[%d]: %.1f°C @ [%d,%d]",
                     i,
                     g_pack_state.temps[i].temperature_c,
                     g_pack_state.temps[i].mux_channel_a,
                     g_pack_state.temps[i].mux_channel_b);
        }
    }
#endif
}

void setup() {
    // Initialize serial logging
    Serial.begin(115200);
    delay(1000);  // Wait for serial to stabilize

    ESP_LOGI(TAG, "ESP32 Battery Monitor Starting...");

    // Initialize system
    error_code_t err = initialize_system();
    if (IS_ERROR(err)) {
        ESP_LOGE(TAG, "System initialization failed!");
        while (1) {
            delay(1000);
            ESP_LOGE(TAG, "CRITICAL: System halted due to initialization failure");
        }
    }

    ESP_LOGI(TAG, "Ready to monitor battery pack");
}

void loop() {
    static uint32_t last_scan = 0;
    uint32_t now = millis();

    // Scan thermistor matrix at configured interval
    if (now - last_scan >= CONFIG_SAMPLE_INTERVAL_MS) {
        last_scan = now;

        // Scan all temperature sensors
        scan_thermistor_matrix();

        // Update pack state
        update_pack_state();

        // Log to serial
        log_pack_state();

        // Store in ring buffer for logging
        ring_buffer_push(&g_data_buffer, &g_pack_state);

        // Handle safety faults
        if (g_pack_state.safety_fault) {
            ESP_LOGW(TAG, "SAFETY FAULT DETECTED! Flags: 0x%04X", 
                     g_pack_state.fault_flags);
            // TODO: Implement safety shutdown logic
        }
    }

    // Small delay to prevent watchdog trigger
    delay(10);
}

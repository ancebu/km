/**
 * @file aht20_driver.cpp
 * @brief AHT20 Temperature & Humidity Sensor Driver Implementation
 */

#include "aht20_driver.h"
#include <Arduino.h>
#include <Wire.h>

// ============================================================================
// INTERNAL STATE FOR DT/Dt CALCULATION
// ============================================================================
static float g_last_temp = 0.0f;
static uint32_t g_last_temp_time_ms = 0;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static error_code_t aht20_write_command(uint8_t address, uint8_t cmd) {
    Wire.beginTransmission(address);
    Wire.write(cmd);
    if (Wire.endTransmission() != 0) {
        return ERR_COMMUNICATION;
    }
    return ERR_OK;
}

static error_code_t aht20_read_bytes(uint8_t address, uint8_t* buffer, size_t len) {
    size_t received = Wire.requestFrom(address, len);
    if (received != len) {
        return ERR_COMMUNICATION;
    }
    for (size_t i = 0; i < len; i++) {
        buffer[i] = Wire.read();
    }
    return ERR_OK;
}

static error_code_t aht20_wait_ready(uint8_t address, uint32_t timeout_ms) {
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        uint8_t status[1];
        error_code_t err = aht20_read_bytes(address, status, 1);
        if (err == ERR_OK && !(status[0] & AHT20_STATUS_BUSY)) {
            return ERR_OK;
        }
        delay(5);
    }
    return ERR_TIMEOUT;
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

error_code_t aht20_init(aht20_state_t* state) {
    if (!state) {
        return ERR_INVALID_ARG;
    }
    
    memset(state, 0, sizeof(aht20_state_t));
    state->i2c_address = AHT20_DEFAULT_ADDRESS;
    
    // Initialize I2C if not already done
    Wire.begin(CONFIG_I2C_SDA_PIN, CONFIG_I2C_SCL_PIN);
    delay(10);
    
    // Send initialization command
    Wire.beginTransmission(state->i2c_address);
    Wire.write(AHT20_CMD_INITIALIZE);
    Wire.write(0x08);  // Initialization parameter
    Wire.write(0x00);  // Initialization parameter
    if (Wire.endTransmission() != 0) {
        return ERR_COMMUNICATION;
    }
    
    delay(10);
    
    // Check calibration status
    uint8_t status[1];
    error_code_t err = aht20_read_bytes(state->i2c_address, status, 1);
    if (err != ERR_OK) {
        return err;
    }
    
    state->initialized = true;
    state->last_read_ms = millis();
    
    return ERR_OK;
}

error_code_t aht20_read(aht20_state_t* state, aht20_data_t* data) {
    if (!state || !data) {
        return ERR_INVALID_ARG;
    }
    
    if (!state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    // Trigger measurement
    Wire.beginTransmission(state->i2c_address);
    Wire.write(AHT20_CMD_TRIGGER);
    Wire.write(0x33);  // Measurement parameter
    Wire.write(0x00);  // Measurement parameter
    if (Wire.endTransmission() != 0) {
        state->error_count++;
        return ERR_COMMUNICATION;
    }
    
    // Wait for measurement to complete (~80ms per datasheet)
    delay(80);
    
    // Read status byte
    uint8_t status_byte;
    error_code_t err = aht20_read_bytes(state->i2c_address, &status_byte, 1);
    if (err != ERR_OK) {
        state->error_count++;
        return err;
    }
    
    // Read 6 bytes of data (status + 3 bytes temp/humidity + CRC)
    uint8_t buffer[7];
    err = aht20_read_bytes(state->i2c_address, buffer, 7);
    if (err != ERR_OK) {
        state->error_count++;
        return err;
    }
    
    // Parse temperature (bytes 3-5, 20-bit resolution)
    // Temperature: ((raw / 2^20) * 200) - 50
    uint32_t temp_raw = ((uint32_t)buffer[3] << 12) | 
                        ((uint32_t)buffer[4] << 4) | 
                        ((uint32_t)buffer[5] >> 4);
    float temp_c = (((float)temp_raw / 1048576.0f) * 200.0f) - 50.0f;
    
    // Parse humidity (bytes 1-2 and 6, 20-bit resolution)
    // Humidity: (raw / 2^20) * 100
    uint32_t hum_raw = ((uint32_t)(buffer[1] & 0x0F) << 16) | 
                       ((uint32_t)buffer[2] << 8) | 
                       (uint32_t)buffer[6];
    float humidity = ((float)hum_raw / 1048576.0f) * 100.0f;
    
    // Apply offsets
    temp_c += state->temp_offset;
    humidity += state->humidity_offset;
    
    // Clamp humidity to valid range
    humidity = constrain(humidity, 0.0f, 100.0f);
    
    // Populate output data
    data->temperature_c = temp_c;
    data->humidity_percent = humidity;
    data->status = status_byte;
    data->valid = true;
    data->timestamp_ms = millis();
    
    // Update derivative calculation
    aht20_calculate_dt_dmin(state, temp_c);
    
    state->last_read_ms = data->timestamp_ms;
    state->error_count = 0;
    
    return ERR_OK;
}

error_code_t aht20_read_nonblocking(aht20_state_t* state, aht20_data_t* data) {
    if (!state || !data) {
        return ERR_INVALID_ARG;
    }
    
    // Check if enough time has passed since last read
    if (millis() - state->last_read_ms < 100) {
        // Return cached data or error if none available
        if (state->last_read_ms == 0) {
            return ERR_TIMEOUT;
        }
        // Could implement caching here if needed
        return ERR_TIMEOUT;
    }
    
    return aht20_read(state, data);
}

float aht20_calculate_dt_dmin(aht20_state_t* state, float new_temp) {
    UNUSED(state);
    
    uint32_t current_time = millis();
    float dt_dmin = 0.0f;
    
    if (g_last_temp_time_ms > 0) {
        uint32_t delta_ms = current_time - g_last_temp_time_ms;
        if (delta_ms > 0) {
            float delta_temp = new_temp - g_last_temp;
            float delta_min = (float)delta_ms / 60000.0f;  // Convert ms to minutes
            if (delta_min > 0.001f) {  // Avoid division by very small numbers
                dt_dmin = delta_temp / delta_min;
            }
        }
    }
    
    // Store for next calculation
    g_last_temp = new_temp;
    g_last_temp_time_ms = current_time;
    
    return dt_dmin;
}

error_code_t aht20_reset(aht20_state_t* state) {
    if (!state) {
        return ERR_INVALID_ARG;
    }
    
    error_code_t err = aht20_write_command(state->i2c_address, AHT20_CMD_RESET);
    if (err != ERR_OK) {
        return err;
    }
    
    delay(20);  // Wait for reset to complete
    
    // Re-initialize
    return aht20_init(state);
}

bool aht20_is_calibrated(aht20_state_t* state) {
    if (!state || !state->initialized) {
        return false;
    }
    
    uint8_t status[1];
    error_code_t err = aht20_read_bytes(state->i2c_address, status, 1);
    if (err != ERR_OK) {
        return false;
    }
    
    return (status[0] & AHT20_STATUS_CALIBRATED) != 0;
}

void aht20_set_temp_offset(aht20_state_t* state, float offset_c) {
    if (state) {
        state->temp_offset = offset_c;
    }
}

float aht20_get_temp_offset(const aht20_state_t* state) {
    if (state) {
        return state->temp_offset;
    }
    return 0.0f;
}

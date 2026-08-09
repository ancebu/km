/**
 * @file ina226_driver.cpp
 * @brief INA226 Current/Voltage/Power Monitor Driver Implementation
 * 
 * Implements the INA226 driver for measuring voltage, current, and power.
 * All sense wires are routed through muxes to the VBUS inputs.
 */

#include "ina226_driver.h"
#include <string.h>
#include <math.h>

// Clock function - can be overridden for testing
#ifndef INA226_CLOCK_FUNC
#define INA226_CLOCK_FUNC default_ina226_clock_ms
static uint32_t default_ina226_clock_ms(void) { return 0; }
#endif

// Register definitions (needed for both Arduino and native builds)
#define INA226_REG_CONFIG       0x00
#define INA226_REG_SHUNT_VOLT   0x01
#define INA226_REG_BUS_VOLT     0x02
#define INA226_REG_POWER        0x03
#define INA226_REG_CURRENT      0x04
#define INA226_REG_CALIBRATION  0x05
#define INA226_REG_MANUFACTURER 0xFE
#define INA226_REG_DIE_ID       0xFF

#ifdef ARDUINO_ARCH_ESP32
#include <Arduino.h>
#include <Wire.h>

// Configuration register bits
#define INA226_CONFIG_RESET     (1 << 15)
#define INA226_CONFIG_AVG_MASK  0x0E00
#define INA226_CONFIG_BRCT_MASK 0x0007
#define INA226_CONFIG_MODE_MASK 0x0007

// Averaging settings
#define INA226_AVG_1            0
#define INA226_AVG_4            1
#define INA226_AVG_16           2
#define INA226_AVG_64           3
#define INA226_AVG_128          4
#define INA226_AVG_256          5
#define INA226_AVG_512          6
#define INA226_AVG_1024         7

// Conversion time settings (in 140us units)
#define INA226_CONV_140US       0
#define INA226_CONV_204US       1
#define INA226_CONV_332US       2
#define INA226_CONV_588US       3
#define INA226_CONV_1100US      4
#define INA226_CONV_2116US      5
#define INA226_CONV_4156US      6
#define INA226_CONV_8244US      7

// Mode settings
#define INA226_MODE_SHUTDOWN    0
#define INA226_MODE_SHUNT_TRIG  1
#define INA226_MODE_BUS_TRIG    2
#define INA226_MODE_SHUNT_BUS_TRIG 3
#define INA226_MODE_SHUNT_CONT  5
#define INA226_MODE_BUS_CONT    6
#define INA226_MODE_SHUNT_BUS_CONT 7

// LSB values
#define INA226_SHUNT_LSB_uV     2.5f    // 2.5 µV per bit
#define INA226_BUS_LSB_mV       1.25f   // 1.25 mV per bit
#define INA226_POWER_LSB_W      25.0f   // 25 µW per bit (when CAL = 40960 / I_LSB)

// Arduino/Wire wrappers
static inline void INA226_WIRE_BEGIN(int sda, int scl) { Wire.begin(sda, scl); }
static inline void INA226_DELAY_MS(uint32_t ms) { delay(ms); }
static inline uint32_t INA226_MILLIS() { return millis(); }

static error_code_t ina226_write_register(uint8_t address, uint8_t reg, uint16_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write((value >> 8) & 0xFF);
    Wire.write(value & 0xFF);
    if (Wire.endTransmission() != 0) {
        return ERR_COMMUNICATION;
    }
    return ERR_OK;
}

static error_code_t ina226_read_register(uint8_t address, uint8_t reg, uint16_t* value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) {
        return ERR_COMMUNICATION;
    }
    
    size_t received = Wire.requestFrom(address, (uint8_t)2);
    if (received != 2) {
        return ERR_COMMUNICATION;
    }
    
    *value = ((uint16_t)Wire.read() << 8) | Wire.read();
    return ERR_OK;
}

#else
// Native test stubs
typedef struct {
    uint16_t registers[6];  // Config, Shunt, Bus, Power, Current, Calibration
    uint16_t manufacturer_id;
    uint16_t die_id;
} ina226_i2c_stub_t;

static ina226_i2c_stub_t s_ina226_stub;

static inline void INA226_WIRE_BEGIN(int sda, int scl) { (void)sda; (void)scl; }
static inline void INA226_DELAY_MS(uint32_t ms) { (void)ms; }
static inline uint32_t INA226_MILLIS() { return 0; }

static error_code_t ina226_write_register(uint8_t address, uint8_t reg, uint16_t value) {
    (void)address;
    if (reg < 6) {
        s_ina226_stub.registers[reg] = value;
    }
    return ERR_OK;
}

static error_code_t ina226_read_register(uint8_t address, uint8_t reg, uint16_t* value) {
    (void)address;
    if (!value) return ERR_INVALID_ARG;
    
    if (reg == INA226_REG_MANUFACTURER) {
        *value = s_ina226_stub.manufacturer_id;
    } else if (reg == INA226_REG_DIE_ID) {
        *value = s_ina226_stub.die_id;
    } else if (reg < 6) {
        *value = s_ina226_stub.registers[reg];
    } else {
        *value = 0;
    }
    return ERR_OK;
}
#endif

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

error_code_t ina226_driver_init(ina226_driver_t* driver,
                                 int i2c_port,
                                 gpio_num_t sda_pin,
                                 gpio_num_t scl_pin,
                                 uint8_t address) {
    if (!driver) {
        return ERR_INVALID_ARG;
    }
    
    memset(driver, 0, sizeof(ina226_driver_t));
    driver->address = address;
    driver->shunt_resistance = CONFIG_INA226_SHUNT_R;
    driver->initialized = false;
    
    // Initialize I2C if not already done
    INA226_WIRE_BEGIN(sda_pin, scl_pin);
    INA226_DELAY_MS(10);
    
    // Reset INA226
    error_code_t err = ina226_write_register(address, INA226_REG_CONFIG, INA226_CONFIG_RESET);
    if (err != ERR_OK) {
        return err;
    }
    
    INA226_DELAY_MS(1);  // Wait for reset to complete
    
    // Calculate default current LSB based on max current
    // I_LSB = Max_Current / 2^15 (for signed 16-bit current register)
    float max_current = CONFIG_INA226_MAX_CURRENT;
    driver->current_lsb = max_current / 32768.0f;
    
    // Ensure minimum LSB of 1µA for reasonable resolution
    if (driver->current_lsb < 0.000001f) {
        driver->current_lsb = 0.000001f;
    }
    
    // Calibration register value: CAL = 0.00512 / (I_LSB * R_SHUNT)
    float cal_value = 0.00512f / (driver->current_lsb * driver->shunt_resistance);
    driver->calibration.calibration_factor = cal_value;
    driver->calibration.shunt_resistance = driver->shunt_resistance;
    driver->calibration.current_lsb = driver->current_lsb;
    driver->calibration.voltage_lsb = INA226_BUS_LSB_mV / 1000.0f;
    driver->calibration.calibrated = false;
    
    // Write calibration register
    err = ina226_write_register(address, INA226_REG_CALIBRATION, (uint16_t)cal_value);
    if (err != ERR_OK) {
        return err;
    }
    
    // Configure for shunt and bus continuous measurement
    uint16_t config = INA226_MODE_SHUNT_BUS_CONT;
    config |= (INA226_AVG_16 << 9);  // Average 16 samples
    config |= (INA226_CONV_588US << 3);  // 588µs conversion time for both
    config |= (INA226_CONV_588US);  // Bus conversion time
    
    err = ina226_write_register(address, INA226_REG_CONFIG, config);
    if (err != ERR_OK) {
        return err;
    }
    
    driver->initialized = true;
    return ERR_OK;
}

error_code_t ina226_driver_read_raw(ina226_driver_t* driver, ina226_data_t* data) {
    if (!driver || !data) {
        return ERR_INVALID_ARG;
    }
    
    if (!driver->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    uint16_t raw_shunt, raw_bus, raw_current, raw_power;
    
    // Read shunt voltage register
    error_code_t err = ina226_read_register(driver->address, INA226_REG_SHUNT_VOLT, &raw_shunt);
    if (err != ERR_OK) {
        return err;
    }
    
    // Read bus voltage register
    err = ina226_read_register(driver->address, INA226_REG_BUS_VOLT, &raw_bus);
    if (err != ERR_OK) {
        return err;
    }
    
    // Read current register
    err = ina226_read_register(driver->address, INA226_REG_CURRENT, &raw_current);
    if (err != ERR_OK) {
        return err;
    }
    
    // Read power register
    err = ina226_read_register(driver->address, INA226_REG_POWER, &raw_power);
    if (err != ERR_OK) {
        return err;
    }
    
    // Convert to physical values
    // Shunt voltage: 2.5 µV per bit (signed)
    float shunt_voltage = (int16_t)raw_shunt * INA226_SHUNT_LSB_uV * 1e-6f;
    
    // Bus voltage: 1.25 mV per bit
    float bus_voltage = raw_bus * INA226_BUS_LSB_mV * 1e-3f;
    
    // Current: I_LSB amperes per bit (signed)
    float current = (int16_t)raw_current * driver->current_lsb;
    
    // Power: 25 µW per bit
    float power = raw_power * INA226_POWER_LSB_W * 1e-6f;
    
    data->shunt_voltage_v = shunt_voltage;
    data->voltage_v = bus_voltage;
    data->current_a = current;
    data->power_w = power;
    data->raw_current = raw_current;
    data->raw_voltage = raw_bus;
    data->timestamp_ms = INA226_CLOCK_FUNC();
    
    return ERR_OK;
}

error_code_t ina226_driver_read(ina226_driver_t* driver, ina226_data_t* data) {
    // For now, read_raw already returns scaled values
    return ina226_driver_read_raw(driver, data);
}

error_code_t ina226_driver_configure(ina226_driver_t* driver,
                                      uint8_t avg_samples,
                                      uint8_t conv_time) {
    if (!driver) {
        return ERR_INVALID_ARG;
    }
    
    if (!driver->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    // Map avg_samples to INA226 setting
    uint8_t avg_setting;
    if (avg_samples <= 1) avg_setting = INA226_AVG_1;
    else if (avg_samples <= 4) avg_setting = INA226_AVG_4;
    else if (avg_samples <= 16) avg_setting = INA226_AVG_16;
    else if (avg_samples <= 64) avg_setting = INA226_AVG_64;
    else if (avg_samples <= 128) avg_setting = INA226_AVG_128;
    else if (avg_samples <= 256) avg_setting = INA226_AVG_256;
    else if (avg_samples <= 512) avg_setting = INA226_AVG_512;
    else avg_setting = INA226_AVG_1024;
    
    // Map conv_time to INA226 setting
    uint8_t conv_setting = conv_time;
    if (conv_setting > 7) conv_setting = 7;
    
    // Build configuration word
    uint16_t config = INA226_MODE_SHUNT_BUS_CONT;
    config |= (avg_setting << 9);
    config |= (conv_setting << 3);
    config |= conv_setting;
    
    return ina226_write_register(driver->address, INA226_REG_CONFIG, config);
}

error_code_t ina226_driver_calibrate(ina226_driver_t* driver,
                                      float shunt_r,
                                      float max_current) {
    if (!driver) {
        return ERR_INVALID_ARG;
    }
    
    if (!driver->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    if (shunt_r <= 0.0f || max_current <= 0.0f) {
        return ERR_INVALID_ARG;
    }
    
    // Calculate current LSB: I_LSB = Max_Current / 2^15
    float current_lsb = max_current / 32768.0f;
    
    // Ensure minimum LSB of 1µA
    if (current_lsb < 0.000001f) {
        current_lsb = 0.000001f;
    }
    
    // Calculate calibration register value: CAL = 0.00512 / (I_LSB * R_SHUNT)
    float cal_value = 0.00512f / (current_lsb * shunt_r);
    uint16_t cal_reg = (uint16_t)cal_value;
    
    // Write calibration register
    error_code_t err = ina226_write_register(driver->address, INA226_REG_CALIBRATION, cal_reg);
    if (err != ERR_OK) {
        return err;
    }
    
    // Update driver state
    driver->shunt_resistance = shunt_r;
    driver->current_lsb = current_lsb;
    driver->calibration.calibration_factor = cal_value;
    driver->calibration.shunt_resistance = shunt_r;
    driver->calibration.current_lsb = current_lsb;
    driver->calibration.voltage_lsb = INA226_BUS_LSB_mV / 1000.0f;
    driver->calibration.calibrated = true;
    
    return ERR_OK;
}

bool ina226_driver_probe(ina226_driver_t* driver) {
    if (!driver || !driver->initialized) {
        return false;
    }
    
    // Read manufacturer ID (should be 0x5449 for TI)
    uint16_t manuf_id;
    error_code_t err = ina226_read_register(driver->address, INA226_REG_MANUFACTURER, &manuf_id);
    if (err != ERR_OK) {
        return false;
    }
    
    if (manuf_id != 0x5449) {
        return false;
    }
    
    // Read die ID (should be 0x2260 or 0x2261)
    uint16_t die_id;
    err = ina226_read_register(driver->address, INA226_REG_DIE_ID, &die_id);
    if (err != ERR_OK) {
        return false;
    }
    
    // INA226 should have die ID 0x226X
    if ((die_id & 0xFFF0) != 0x2260) {
        return false;
    }
    
    return true;
}

error_code_t ina226_driver_reset(ina226_driver_t* driver) {
    if (!driver) {
        return ERR_INVALID_ARG;
    }
    
    if (!driver->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    // Send reset command
    error_code_t err = ina226_write_register(driver->address, INA226_REG_CONFIG, INA226_CONFIG_RESET);
    if (err != ERR_OK) {
        return err;
    }
    
    delay(1);  // Wait for reset
    
    // Re-initialize with default configuration
    uint16_t config = INA226_MODE_SHUNT_BUS_CONT;
    config |= (INA226_AVG_16 << 9);
    config |= (INA226_CONV_588US << 3);
    config |= INA226_CONV_588US;
    
    return ina226_write_register(driver->address, INA226_REG_CONFIG, config);
}

error_code_t ina226_driver_deinit(ina226_driver_t* driver) {
    if (!driver) {
        return ERR_INVALID_ARG;
    }
    
    // Put INA226 in shutdown mode
    error_code_t err = ina226_write_register(driver->address, INA226_REG_CONFIG, INA226_MODE_SHUTDOWN);
    if (err != ERR_OK) {
        return err;
    }
    
    driver->initialized = false;
    return ERR_OK;
}

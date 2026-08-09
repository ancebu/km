/**
 * @file ina226_driver.h
 * @brief INA226 current/voltage/power monitor driver
 * 
 * Interfaces with the INA226 to measure voltage, current, and power.
 * All sense wires are routed through muxes to the VBUS inputs.
 */

#pragma once

#include "types.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief INA226 driver state
 */
typedef struct {
    uint8_t address;                      // I2C address
    float shunt_resistance;               // Shunt resistor value (Ω)
    float current_lsb;                    // Current LSB (A/bit)
    ina226_calibration_t calibration;     // Calibration data
    bool initialized;                     // true if initialized
} ina226_driver_t;

/**
 * @brief Initialize INA226 driver
 * @param driver Pointer to driver state
 * @param i2c_port I2C port number (0 or 1)
 * @param sda_pin SDA GPIO pin (int for native compatibility)
 * @param scl_pin SCL GPIO pin (int for native compatibility)
 * @param address INA226 I2C address
 * @return ERR_OK on success
 */
error_code_t ina226_driver_init(ina226_driver_t* driver,
                                 int i2c_port,
                                 int sda_pin,
                                 int scl_pin,
                                 uint8_t address);

/**
 * @brief Read raw measurements from INA226
 * @param driver Pointer to driver state
 * @param data Output: measurement data
 * @return ERR_OK on success
 */
error_code_t ina226_driver_read_raw(ina226_driver_t* driver, ina226_data_t* data);

/**
 * @brief Read and process measurements (scaled values)
 * @param driver Pointer to driver state
 * @param data Output: processed measurement data
 * @return ERR_OK on success
 */
error_code_t ina226_driver_read(ina226_driver_t* driver, ina226_data_t* data);

/**
 * @brief Configure INA226 averaging and conversion time
 * @param driver Pointer to driver state
 * @param avg_samples Number of samples to average
 * @param conv_time Conversion time setting
 * @return ERR_OK on success
 */
error_code_t ina226_driver_configure(ina226_driver_t* driver,
                                      uint8_t avg_samples,
                                      uint8_t conv_time);

/**
 * @brief Calibrate INA226 for accurate current measurement
 * @param driver Pointer to driver state
 * @param shunt_r Shunt resistance in ohms
 * @param max_current Maximum expected current
 * @return ERR_OK on success
 */
error_code_t ina226_driver_calibrate(ina226_driver_t* driver,
                                      float shunt_r,
                                      float max_current);

/**
 * @brief Check if INA226 is present on I2C bus
 * @param driver Pointer to driver state
 * @return true if device found
 */
bool ina226_driver_probe(ina226_driver_t* driver);

/**
 * @brief Reset INA226 to default state
 * @param driver Pointer to driver state
 * @return ERR_OK on success
 */
error_code_t ina226_driver_reset(ina226_driver_t* driver);

/**
 * @brief Deinitialize INA226 driver
 * @param driver Pointer to driver state
 * @return ERR_OK on success
 */
error_code_t ina226_driver_deinit(ina226_driver_t* driver);

#ifdef __cplusplus
}
#endif

/**
 * @file mux_controller.h
 * @brief Dual multiplexer control for thermistor matrix
 * 
 * Controls two multiplexers to route any sense wire pair to the INA226.
 * MUX A routes positive lines, MUX B routes negative lines.
 */

#pragma once

#include "types.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mux controller state
 */
typedef struct {
    uint8_t current_channel_a;
    uint8_t current_channel_b;
    bool initialized;
    bool enabled;
} mux_controller_t;

/**
 * @brief Initialize the mux controller
 * @param controller Pointer to controller state
 * @return ERR_OK on success
 */
error_code_t mux_controller_init(mux_controller_t* controller);

/**
 * @brief Set multiplexer channel selection
 * @param controller Pointer to controller state
 * @param channel_a MUX A channel (0 to CONFIG_MUX_CHANNELS_A-1)
 * @param channel_b MUX B channel (0 to CONFIG_MUX_CHANNELS_B-1)
 * @return ERR_OK on success
 */
error_code_t mux_controller_set_channels(mux_controller_t* controller, 
                                          uint8_t channel_a, 
                                          uint8_t channel_b);

/**
 * @brief Enable multiplexers
 * @param controller Pointer to controller state
 * @return ERR_OK on success
 */
error_code_t mux_controller_enable(mux_controller_t* controller);

/**
 * @brief Disable multiplexers (high impedance)
 * @param controller Pointer to controller state
 * @return ERR_OK on success
 */
error_code_t mux_controller_disable(mux_controller_t* controller);

/**
 * @brief Get current channel selection
 * @param controller Pointer to controller state
 * @param channel_a Output: current MUX A channel
 * @param channel_b Output: current MUX B channel
 * @return ERR_OK on success
 */
error_code_t mux_controller_get_channels(const mux_controller_t* controller,
                                          uint8_t* channel_a,
                                          uint8_t* channel_b);

/**
 * @brief Scan all channel combinations
 * @param controller Pointer to controller state
 * @param callback Function called for each channel pair
 * @param user_data User data passed to callback
 * @return ERR_OK on success
 */
typedef void (*mux_scan_callback_t)(uint8_t channel_a, uint8_t channel_b, void* user_data);
error_code_t mux_controller_scan_all(mux_controller_t* controller,
                                      mux_scan_callback_t callback,
                                      void* user_data);

/**
 * @brief Wait for mux settling after channel change
 */
void mux_controller_wait_settling(void);

/**
 * @brief Deinitialize mux controller
 * @param controller Pointer to controller state
 * @return ERR_OK on success
 */
error_code_t mux_controller_deinit(mux_controller_t* controller);

#ifdef __cplusplus
}
#endif

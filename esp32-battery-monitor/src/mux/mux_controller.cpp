/**
 * @file mux_controller.cpp
 * @brief Implementation of dual multiplexer control
 */

#include "mux_controller.h"
#include "config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef ARDUINO_ARCH_ESP32
#include <Arduino.h>
#include <esp_log.h>
static const char* TAG = "MUX_CTRL";
#else
// Native stubs for logging
#define ESP_LOGI(tag, ...) do {} while(0)
#define ESP_LOGD(tag, ...) do {} while(0)
#define ESP_LOGE(tag, ...) do {} while(0)
#endif

// Pin definitions from config
#ifdef ARDUINO_ARCH_ESP32
static const gpio_num_t mux_a_pins[] = {
    CONFIG_MUX_A_SELECT_0,
    CONFIG_MUX_A_SELECT_1,
    CONFIG_MUX_A_SELECT_2
};

static const gpio_num_t mux_b_pins[] = {
    CONFIG_MUX_B_SELECT_0,
    CONFIG_MUX_B_SELECT_1,
    CONFIG_MUX_B_SELECT_2
};

static const gpio_num_t mux_a_enable = CONFIG_MUX_A_ENABLE;
static const gpio_num_t mux_b_enable = CONFIG_MUX_B_ENABLE;
#endif

// HAL implementations
#ifdef ARDUINO_ARCH_ESP32
static void arduino_pin_mode(uint8_t pin, uint8_t mode) { pinMode(pin, mode); }
static void arduino_digital_write(uint8_t pin, uint8_t val) { digitalWrite(pin, val); }
static void arduino_delay_ms(uint32_t ms) { delay(ms); }
const gpio_hal_t gpio_hal_arduino = { arduino_pin_mode, arduino_digital_write, arduino_delay_ms };
#else
static void noop_pin_mode(uint8_t pin, uint8_t mode) { (void)pin; (void)mode; }
static void noop_digital_write(uint8_t pin, uint8_t val) { (void)pin; (void)val; }
static void noop_delay_ms(uint32_t ms) { (void)ms; }
const gpio_hal_t gpio_hal_native = { noop_pin_mode, noop_digital_write, noop_delay_ms };
#endif

// Default HAL accessor
static const gpio_hal_t* get_default_hal(void) {
#ifdef ARDUINO_ARCH_ESP32
    return &gpio_hal_arduino;
#else
    return &gpio_hal_native;
#endif
}

error_code_t mux_controller_init(mux_controller_t* controller, const gpio_hal_t* hal) {
    if (controller == NULL) {
        return ERR_INVALID_ARG;
    }

    // Use provided HAL or default
    if (hal == NULL) {
        hal = get_default_hal();
    }
    controller->hal = hal;

    ESP_LOGI("MUX_CTRL", "Initializing mux controller");

    // Configure MUX A select pins
    for (int i = 0; i < 3; i++) {
        controller->hal->pin_mode(mux_a_pins[i], OUTPUT);
        controller->hal->digital_write(mux_a_pins[i], LOW);
    }

    // Configure MUX B select pins
    for (int i = 0; i < 3; i++) {
        controller->hal->pin_mode(mux_b_pins[i], OUTPUT);
        controller->hal->digital_write(mux_b_pins[i], LOW);
    }

    // Configure enable pins (active low)
    controller->hal->pin_mode(mux_a_enable, OUTPUT);
    controller->hal->pin_mode(mux_b_enable, OUTPUT);
    controller->hal->digital_write(mux_a_enable, HIGH);  // Start disabled
    controller->hal->digital_write(mux_b_enable, HIGH);

    controller->current_channel_a = 0;
    controller->current_channel_b = 0;
    controller->initialized = true;
    controller->enabled = false;

    ESP_LOGI("MUX_CTRL", "Mux controller initialized successfully");
    return ERR_OK;
}

error_code_t mux_controller_set_channels(mux_controller_t* controller,
                                          uint8_t channel_a,
                                          uint8_t channel_b) {
    if (controller == NULL || !controller->initialized) {
        return ERR_NOT_INITIALIZED;
    }

    if (channel_a >= CONFIG_MUX_CHANNELS_A || channel_b >= CONFIG_MUX_CHANNELS_B) {
        return ERR_OUT_OF_RANGE;
    }

    // Set MUX A channels
    for (int i = 0; i < 3; i++) {
        controller->hal->digital_write(mux_a_pins[i], (channel_a >> i) & 0x01);
    }

    // Set MUX B channels
    for (int i = 0; i < 3; i++) {
        controller->hal->digital_write(mux_b_pins[i], (channel_b >> i) & 0x01);
    }

    controller->current_channel_a = channel_a;
    controller->current_channel_b = channel_b;

    // Wait for signal settling
    mux_controller_wait_settling(controller);

    ESP_LOGD("MUX_CTRL", "Mux channels set: A=%d, B=%d", channel_a, channel_b);
    return ERR_OK;
}

error_code_t mux_controller_enable(mux_controller_t* controller) {
    if (controller == NULL || !controller->initialized) {
        return ERR_NOT_INITIALIZED;
    }

    controller->hal->digital_write(mux_a_enable, LOW);   // Active low
    controller->hal->digital_write(mux_b_enable, LOW);
    controller->enabled = true;

    ESP_LOGD("MUX_CTRL", "Mux controllers enabled");
    return ERR_OK;
}

error_code_t mux_controller_disable(mux_controller_t* controller) {
    if (controller == NULL || !controller->initialized) {
        return ERR_NOT_INITIALIZED;
    }

    controller->hal->digital_write(mux_a_enable, HIGH);  // Active low
    controller->hal->digital_write(mux_b_enable, HIGH);
    controller->enabled = false;

    ESP_LOGD("MUX_CTRL", "Mux controllers disabled");
    return ERR_OK;
}

error_code_t mux_controller_get_channels(const mux_controller_t* controller,
                                          uint8_t* channel_a,
                                          uint8_t* channel_b) {
    if (controller == NULL || channel_a == NULL || channel_b == NULL) {
        return ERR_INVALID_ARG;
    }

    *channel_a = controller->current_channel_a;
    *channel_b = controller->current_channel_b;
    return ERR_OK;
}

error_code_t mux_controller_scan_all(mux_controller_t* controller,
                                      mux_scan_callback_t callback,
                                      void* user_data) {
    if (controller == NULL || callback == NULL || !controller->initialized) {
        return ERR_INVALID_ARG;
    }

    error_code_t err;

    // Enable muxes for scanning
    err = mux_controller_enable(controller);
    if (IS_ERROR(err)) {
        return err;
    }

    // Scan all combinations
    for (uint8_t a = 0; a < CONFIG_MUX_CHANNELS_A; a++) {
        for (uint8_t b = 0; b < CONFIG_MUX_CHANNELS_B; b++) {
            err = mux_controller_set_channels(controller, a, b);
            if (IS_ERROR(err)) {
                ESP_LOGE("MUX_CTRL", "Failed to set channels %d,%d: %s", 
                         a, b, error_code_to_string(err));
                continue;
            }

            callback(a, b, user_data);
        }
    }

    // Disable after scanning
    return mux_controller_disable(controller);
}

void mux_controller_wait_settling(const mux_controller_t* controller) {
    controller->hal->delay_ms(CONFIG_MUX_SETTLING_MS);
}

error_code_t mux_controller_deinit(mux_controller_t* controller) {
    if (controller == NULL) {
        return ERR_INVALID_ARG;
    }

    // Disable muxes first
    mux_controller_disable(controller);

    // Set all pins to input (high impedance)
    for (int i = 0; i < 3; i++) {
        controller->hal->pin_mode(mux_a_pins[i], INPUT);
        controller->hal->pin_mode(mux_b_pins[i], INPUT);
    }
    controller->hal->pin_mode(mux_a_enable, INPUT);
    controller->hal->pin_mode(mux_b_enable, INPUT);

    controller->initialized = false;
    controller->enabled = false;

    ESP_LOGI("MUX_CTRL", "Mux controller deinitialized");
    return ERR_OK;
}

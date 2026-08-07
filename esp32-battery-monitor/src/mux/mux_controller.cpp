/**
 * @file mux_controller.cpp
 * @brief Implementation of dual multiplexer control
 */

#include "mux_controller.h"
#include "config.h"
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "MUX_CTRL";

// Pin definitions from config
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

error_code_t mux_controller_init(mux_controller_t* controller) {
    if (controller == NULL) {
        return ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing mux controller");

    // Configure MUX A select pins
    for (int i = 0; i < 3; i++) {
        pinMode(mux_a_pins[i], OUTPUT);
        digitalWrite(mux_a_pins[i], LOW);
    }

    // Configure MUX B select pins
    for (int i = 0; i < 3; i++) {
        pinMode(mux_b_pins[i], OUTPUT);
        digitalWrite(mux_b_pins[i], LOW);
    }

    // Configure enable pins (active low)
    pinMode(mux_a_enable, OUTPUT);
    pinMode(mux_b_enable, OUTPUT);
    digitalWrite(mux_a_enable, HIGH);  // Start disabled
    digitalWrite(mux_b_enable, HIGH);

    controller->current_channel_a = 0;
    controller->current_channel_b = 0;
    controller->initialized = true;
    controller->enabled = false;

    ESP_LOGI(TAG, "Mux controller initialized successfully");
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
        digitalWrite(mux_a_pins[i], (channel_a >> i) & 0x01);
    }

    // Set MUX B channels
    for (int i = 0; i < 3; i++) {
        digitalWrite(mux_b_pins[i], (channel_b >> i) & 0x01);
    }

    controller->current_channel_a = channel_a;
    controller->current_channel_b = channel_b;

    // Wait for signal settling
    mux_controller_wait_settling();

    ESP_LOGD(TAG, "Mux channels set: A=%d, B=%d", channel_a, channel_b);
    return ERR_OK;
}

error_code_t mux_controller_enable(mux_controller_t* controller) {
    if (controller == NULL || !controller->initialized) {
        return ERR_NOT_INITIALIZED;
    }

    digitalWrite(mux_a_enable, LOW);   // Active low
    digitalWrite(mux_b_enable, LOW);
    controller->enabled = true;

    ESP_LOGD(TAG, "Mux controllers enabled");
    return ERR_OK;
}

error_code_t mux_controller_disable(mux_controller_t* controller) {
    if (controller == NULL || !controller->initialized) {
        return ERR_NOT_INITIALIZED;
    }

    digitalWrite(mux_a_enable, HIGH);  // Active low
    digitalWrite(mux_b_enable, HIGH);
    controller->enabled = false;

    ESP_LOGD(TAG, "Mux controllers disabled");
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
                ESP_LOGE(TAG, "Failed to set channels %d,%d: %s", 
                         a, b, error_code_to_string(err));
                continue;
            }

            callback(a, b, user_data);
        }
    }

    // Disable after scanning
    return mux_controller_disable(controller);
}

void mux_controller_wait_settling(void) {
    delay(CONFIG_MUX_SETTLING_MS);
}

error_code_t mux_controller_deinit(mux_controller_t* controller) {
    if (controller == NULL) {
        return ERR_INVALID_ARG;
    }

    // Disable muxes first
    mux_controller_disable(controller);

    // Set all pins to input (high impedance)
    for (int i = 0; i < 3; i++) {
        pinMode(mux_a_pins[i], INPUT);
        pinMode(mux_b_pins[i], INPUT);
    }
    pinMode(mux_a_enable, INPUT);
    pinMode(mux_b_enable, INPUT);

    controller->initialized = false;
    controller->enabled = false;

    ESP_LOGI(TAG, "Mux controller deinitialized");
    return ERR_OK;
}

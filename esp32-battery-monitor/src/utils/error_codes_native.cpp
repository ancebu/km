/**
 * @file error_codes_native.cpp
 * @brief Native (non-ESP32) implementation of error code utilities
 * 
 * This file provides implementations of error code functions for the
 * native test environment, without requiring Arduino/ESP32 headers.
 */

#ifndef ARDUINO_ARCH_ESP32
    // ESP32 implementation would be elsewhere
#else
    // Native implementation for unit testing

#include "types.h"

const char* error_code_to_string(error_code_t err) {
    switch (err) {
        case ERR_OK:
            return "ERR_OK";
        case ERR_INVALID_ARG:
            return "ERR_INVALID_ARG";
        case ERR_TIMEOUT:
            return "ERR_TIMEOUT";
        case ERR_NOT_INITIALIZED:
            return "ERR_NOT_INITIALIZED";
        case ERR_OUT_OF_RANGE:
            return "ERR_OUT_OF_RANGE";
        case ERR_COMMUNICATION:
            return "ERR_COMMUNICATION";
        case ERR_CALIBRATION_FAILED:
            return "ERR_CALIBRATION_FAILED";
        case ERR_HARDWARE_FAULT:
            return "ERR_HARDWARE_FAULT";
        case ERR_MEMORY_ALLOC:
            return "ERR_MEMORY_ALLOC";
        case ERR_UNSUPPORTED:
            return "ERR_UNSUPPORTED";
        case ERR_SAFETY_TRIP:
            return "ERR_SAFETY_TRIP";
        case ERR_UNKNOWN:
            return "ERR_UNKNOWN";
        default:
            return "ERR_UNDEFINED";
    }
}

#endif // ARDUINO_ARCH_ESP32

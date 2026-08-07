/**
 * @file error_codes.h
 * @brief Error code definitions and utilities
 * 
 * This file is a lightweight wrapper around types.h error codes.
 * Included for backward compatibility and explicit error handling.
 */

#pragma once

#include "types.h"

// Re-export error codes for convenience
typedef error_code_t err_t;

#define ERR_SUCCESS         ERR_OK

// Error handling macros
#define RETURN_IF_ERROR(expr) do { \
    error_code_t _err = (expr); \
    if (IS_ERROR(_err)) { \
        return _err; \
    } \
} while(0)

#define RETURN_VAL_IF_ERROR(expr, val) do { \
    error_code_t _err = (expr); \
    if (IS_ERROR(_err)) { \
        return (val); \
    } \
} while(0)

#define LOG_ERROR(err, msg) do { \
    if (IS_ERROR(err)) { \
        ESP_LOGE("ERROR", "%s: %s", msg, error_code_to_string(err)); \
    } \
} while(0)

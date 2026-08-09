/**
 * @file lcd_1602_i2c.h
 * @brief I2C 1602 LCD 16x2 Display Driver
 * 
 * Driver for HD44780-based 16x2 LCD displays with PCF8574 I2C backpack.
 * Commonly available as "I2C LCD 1602" modules.
 */

#pragma once

#include "types.h"
#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// LCD 1602 I2C CONFIGURATION
// ============================================================================
#define LCD_1602_DEFAULT_ADDRESS    0x27    // Default I2C address (some use 0x3F)
#define LCD_1602_ALTERNATE_ADDRESS  0x3F    // Alternate I2C address
#define LCD_1602_COLS               16      // Number of columns
#define LCD_1602_ROWS               2       // Number of rows

// ============================================================================
// LCD HAL ABSTRACTION
// ============================================================================

/**
 * @brief LCD HAL interface for testability
 */
typedef struct {
    void (*begin)(int sda, int scl);
    void (*begin_transmission)(uint8_t addr);
    size_t (*write)(uint8_t data);
    uint8_t (*end_transmission)(void);
    void (*delay_ms)(uint32_t ms);
    void (*delay_us)(uint32_t us);
} lcd_hal_t;

// Default HAL implementations
extern const lcd_hal_t lcd_hal_arduino;
extern const lcd_hal_t lcd_hal_native;

// HD44780 command definitions
#define LCD_CMD_CLEAR_DISPLAY       0x01
#define LCD_CMD_RETURN_HOME         0x02
#define LCD_CMD_ENTRY_MODE          0x04
#define LCD_CMD_DISPLAY_CONTROL     0x08
#define LCD_CMD_FUNCTION_SET        0x20
#define LCD_CMD_SET_CGRAM_ADDR      0x40
#define LCD_CMD_SET_DDRAM_ADDR      0x80

// Entry mode flags
#define LCD_ENTRY_INCREMENT         0x02
#define LCD_ENTRY_DECREMENT         0x00
#define LCD_ENTRY_SHIFT_ON          0x01
#define LCD_ENTRY_SHIFT_OFF         0x00

// Display control flags
#define LCD_DISPLAY_ON              0x04
#define LCD_DISPLAY_OFF             0x00
#define LCD_CURSOR_ON               0x02
#define LCD_CURSOR_OFF              0x00
#define LCD_BLINK_ON                0x01
#define LCD_BLINK_OFF               0x00

// Function set flags
#define LCD_FUNCTION_8BIT           0x10
#define LCD_FUNCTION_4BIT           0x00
#define LCD_FUNCTION_2LINE          0x08
#define LCD_FUNCTION_1LINE          0x00
#define LCD_FUNCTION_5X10           0x04
#define LCD_FUNCTION_5X8            0x00

// PCF8574 backpack bit mappings
#define LCD_BACKLIGHT               0x08    // Backlight control bit
#define LCD_EN                      0x04    // Enable bit
#define LCD_RW                      0x02    // Read/Write bit (always write)
#define LCD_RS                      0x01    // Register Select bit

// Custom character storage
#define LCD_CGRAM_SIZE              8       // Number of custom characters
#define LCD_CHAR_HEIGHT             8       // Pixels per character row

// ============================================================================
// DATA TYPES
// ============================================================================

/**
 * @brief LCD display state
 */
typedef struct {
    bool initialized;             // true if LCD is initialized
    uint8_t i2c_address;          // I2C address
    uint8_t display_function;     // Function set flags
    uint8_t display_control;      // Display control flags
    uint8_t display_mode;         // Entry mode flags
    uint8_t backlight_state;      // Backlight on/off
    int8_t col;                   // Current cursor column
    int8_t row;                   // Current cursor row
    uint8_t custom_chars[LCD_CGRAM_SIZE]; // Custom character definitions
    const lcd_hal_t* hal;         // HAL interface
} lcd_1602_state_t;

/**
 * @brief Text alignment options
 */
typedef enum {
    LCD_ALIGN_LEFT = 0,
    LCD_ALIGN_CENTER,
    LCD_ALIGN_RIGHT
} lcd_alignment_t;

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Initialize LCD 1602 I2C display
 * @param state Pointer to driver state structure
 * @param address I2C address (use 0 for auto-detect)
 * @param hal LCD HAL implementation (use NULL for default)
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_init(lcd_1602_state_t* state, uint8_t address, const lcd_hal_t* hal);

/**
 * @brief Clear the display and return cursor to home position
 * @param state Pointer to driver state structure
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_clear(lcd_1602_state_t* state);

/**
 * @brief Set cursor position
 * @param state Pointer to driver state structure
 * @param col Column (0-15)
 * @param row Row (0-1)
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_set_cursor(lcd_1602_state_t* state, uint8_t col, uint8_t row);

/**
 * @brief Turn display on/off
 * @param state Pointer to driver state structure
 * @param enable true to turn display on
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_display_enable(lcd_1602_state_t* state, bool enable);

/**
 * @brief Turn cursor on/off
 * @param state Pointer to driver state structure
 * @param enable true to show cursor
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_cursor_enable(lcd_1602_state_t* state, bool enable);

/**
 * @brief Turn cursor blink on/off
 * @param state Pointer to driver state structure
 * @param enable true to enable blinking
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_blink_enable(lcd_1602_state_t* state, bool enable);

/**
 * @brief Turn backlight on/off
 * @param state Pointer to driver state structure
 * @param enable true to turn backlight on
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_backlight_enable(lcd_1602_state_t* state, bool enable);

/**
 * @brief Print a string at current cursor position
 * @param state Pointer to driver state structure
 * @param str Null-terminated string to print
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_print(lcd_1602_state_t* state, const char* str);

/**
 * @brief Print a string at specified position
 * @param state Pointer to driver state structure
 * @param col Column (0-15)
 * @param row Row (0-1)
 * @param str Null-terminated string to print
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_print_at(lcd_1602_state_t* state, uint8_t col, uint8_t row, const char* str);

/**
 * @brief Print a string with alignment
 * @param state Pointer to driver state structure
 * @param row Row (0-1)
 * @param str Null-terminated string to print
 * @param alignment Text alignment (LEFT, CENTER, RIGHT)
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_print_aligned(lcd_1602_state_t* state, uint8_t row, const char* str, lcd_alignment_t alignment);

/**
 * @brief Print an integer value
 * @param state Pointer to driver state structure
 * @param value Integer to print
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_print_int(lcd_1602_state_t* state, int value);

/**
 * @brief Print a float value
 * @param state Pointer to driver state structure
 * @param value Float to print
 * @param decimals Number of decimal places
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_print_float(lcd_1602_state_t* state, float value, uint8_t decimals);

/**
 * @brief Create a custom character
 * @param state Pointer to driver state structure
 * @param slot Character slot (0-7)
 * @param bitmap 8-byte array defining the character pattern
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_create_char(lcd_1602_state_t* state, uint8_t slot, const uint8_t* bitmap);

/**
 * @brief Scroll display left
 * @param state Pointer to driver state structure
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_scroll_left(lcd_1602_state_t* state);

/**
 * @brief Scroll display right
 * @param state Pointer to driver state structure
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_scroll_right(lcd_1602_state_t* state);

/**
 * @brief Stop scrolling display
 * @param state Pointer to driver state structure
 * @return ERR_OK on success, error code otherwise
 */
error_code_t lcd_1602_stop_scroll(lcd_1602_state_t* state);

/**
 * @brief Auto-detect LCD I2C address
 * @param state Pointer to driver state structure (output: detected address)
 * @return ERR_OK if found, ERR_NOT_FOUND otherwise
 */
error_code_t lcd_1602_auto_detect_address(lcd_1602_state_t* state);

#ifdef __cplusplus
}
#endif

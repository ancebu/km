/**
 * @file lcd_1602_i2c.cpp
 * @brief I2C 1602 LCD 16x2 Display Driver Implementation
 * 
 * Implements driver for HD44780-based 16x2 LCD with PCF8574 I2C backpack.
 */

#include "lcd_1602_i2c.h"
#include <Arduino.h>
#include <Wire.h>

// ============================================================================
// INTERNAL HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Send a byte to the LCD (command or data)
 */
static void lcd_1602_send_byte(uint8_t address, uint8_t byte, uint8_t mode) {
    // Split byte into high and low nibbles for 4-bit mode
    uint8_t high_nibble = byte & 0xF0;
    uint8_t low_nibble = (byte << 4) & 0xF0;
    
    // Send high nibble
    Wire.beginTransmission(address);
    Wire.write(high_nibble | mode | LCD_BACKLIGHT);
    Wire.write(high_nibble | mode | LCD_BACKLIGHT | LCD_EN);  // Enable high
    delayMicroseconds(2);
    Wire.write(high_nibble | mode | LCD_BACKLIGHT);           // Enable low
    delayMicroseconds(2);
    Wire.endTransmission();
    
    // Send low nibble
    Wire.beginTransmission(address);
    Wire.write(low_nibble | mode | LCD_BACKLIGHT);
    Wire.write(low_nibble | mode | LCD_BACKLIGHT | LCD_EN);   // Enable high
    delayMicroseconds(2);
    Wire.write(low_nibble | mode | LCD_BACKLIGHT);            // Enable low
    delayMicroseconds(2);
    Wire.endTransmission();
    
    delayMicroseconds(50);  // Wait for command to execute
}

/**
 * @brief Send a command to the LCD
 */
static void lcd_1602_send_command(uint8_t address, uint8_t cmd) {
    lcd_1602_send_byte(address, cmd, 0);  // RS = 0 for command
}

/**
 * @brief Send data to the LCD
 */
static void lcd_1602_send_data(uint8_t address, uint8_t data) {
    lcd_1602_send_byte(address, data, LCD_RS);  // RS = 1 for data
}

/**
 * @brief Initialize LCD in 4-bit mode
 */
static error_code_t lcd_1602_init_4bit(uint8_t address) {
    delay(50);  // Wait for LCD to power up
    
    // Send function set multiple times to ensure 4-bit mode
    // First three times as 8-bit (only high nibble matters)
    Wire.beginTransmission(address);
    Wire.write(0x30 | LCD_BACKLIGHT);
    Wire.write(0x30 | LCD_BACKLIGHT | LCD_EN);
    delayMicroseconds(5);
    Wire.write(0x30 | LCD_BACKLIGHT);
    Wire.endTransmission();
    delayMicroseconds(5);
    
    Wire.beginTransmission(address);
    Wire.write(0x30 | LCD_BACKLIGHT);
    Wire.write(0x30 | LCD_BACKLIGHT | LCD_EN);
    delayMicroseconds(5);
    Wire.write(0x30 | LCD_BACKLIGHT);
    Wire.endTransmission();
    delayMicroseconds(5);
    
    Wire.beginTransmission(address);
    Wire.write(0x30 | LCD_BACKLIGHT);
    Wire.write(0x30 | LCD_BACKLIGHT | LCD_EN);
    delayMicroseconds(5);
    Wire.write(0x30 | LCD_BACKLIGHT);
    Wire.endTransmission();
    delayMicroseconds(5);
    
    // Now set to 4-bit mode with 2 lines
    Wire.beginTransmission(address);
    Wire.write(0x20 | LCD_BACKLIGHT);
    Wire.write(0x20 | LCD_BACKLIGHT | LCD_EN);
    delayMicroseconds(5);
    Wire.write(0x20 | LCD_BACKLIGHT);
    Wire.endTransmission();
    delayMicroseconds(5);
    
    return ERR_OK;
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

error_code_t lcd_1602_init(lcd_1602_state_t* state, uint8_t address) {
    if (!state) {
        return ERR_INVALID_ARG;
    }
    
    memset(state, 0, sizeof(lcd_1602_state_t));
    
    // Auto-detect address if requested
    if (address == 0) {
        error_code_t err = lcd_1602_auto_detect_address(state);
        if (err != ERR_OK) {
            return err;
        }
        address = state->i2c_address;
    } else {
        state->i2c_address = address;
    }
    
    // Initialize I2C if not already done
    Wire.begin(CONFIG_I2C_SDA_PIN, CONFIG_I2C_SCL_PIN);
    delay(10);
    
    // Initialize LCD in 4-bit mode
    error_code_t err = lcd_1602_init_4bit(address);
    if (err != ERR_OK) {
        return err;
    }
    
    // Set display function: 4-bit mode, 2 lines, 5x8 dots
    state->display_function = LCD_FUNCTION_4BIT | LCD_FUNCTION_2LINE | LCD_FUNCTION_5X8;
    lcd_1602_send_command(address, LCD_CMD_FUNCTION_SET | state->display_function);
    delayMicroseconds(50);
    
    // Set display control: display on, cursor off, blink off
    state->display_control = LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF;
    lcd_1602_send_command(address, LCD_CMD_DISPLAY_CONTROL | state->display_control);
    delayMicroseconds(50);
    
    // Set entry mode: increment, no shift
    state->display_mode = LCD_ENTRY_INCREMENT | LCD_ENTRY_SHIFT_OFF;
    lcd_1602_send_command(address, LCD_CMD_ENTRY_MODE | state->display_mode);
    delayMicroseconds(50);
    
    // Clear display
    lcd_1602_send_command(address, LCD_CMD_CLEAR_DISPLAY);
    delayMicroseconds(2000);  // Clear takes longer
    
    // Return home
    lcd_1602_send_command(address, LCD_CMD_RETURN_HOME);
    delayMicroseconds(2000);
    
    // Initialize state
    state->initialized = true;
    state->backlight_state = 1;
    state->col = 0;
    state->row = 0;
    
    return ERR_OK;
}

error_code_t lcd_1602_clear(lcd_1602_state_t* state) {
    if (!state || !state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    lcd_1602_send_command(state->i2c_address, LCD_CMD_CLEAR_DISPLAY);
    delayMicroseconds(2000);
    
    state->col = 0;
    state->row = 0;
    
    return ERR_OK;
}

error_code_t lcd_1602_set_cursor(lcd_1602_state_t* state, uint8_t col, uint8_t row) {
    if (!state || !state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    // Clamp values to valid range
    col = (col >= LCD_1602_COLS) ? LCD_1602_COLS - 1 : col;
    row = (row >= LCD_1602_ROWS) ? LCD_1602_ROWS - 1 : row;
    
    // Calculate DDRAM address based on row
    uint8_t addr;
    switch (row) {
        case 0:
            addr = 0x00 + col;
            break;
        case 1:
            addr = 0x40 + col;
            break;
        default:
            addr = col;
            break;
    }
    
    lcd_1602_send_command(state->i2c_address, LCD_CMD_SET_DDRAM_ADDR | addr);
    delayMicroseconds(50);
    
    state->col = col;
    state->row = row;
    
    return ERR_OK;
}

error_code_t lcd_1602_display_enable(lcd_1602_state_t* state, bool enable) {
    if (!state || !state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    if (enable) {
        state->display_control |= LCD_DISPLAY_ON;
    } else {
        state->display_control &= ~LCD_DISPLAY_ON;
    }
    
    lcd_1602_send_command(state->i2c_address, LCD_CMD_DISPLAY_CONTROL | state->display_control);
    delayMicroseconds(50);
    
    return ERR_OK;
}

error_code_t lcd_1602_cursor_enable(lcd_1602_state_t* state, bool enable) {
    if (!state || !state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    if (enable) {
        state->display_control |= LCD_CURSOR_ON;
    } else {
        state->display_control &= ~LCD_CURSOR_ON;
    }
    
    lcd_1602_send_command(state->i2c_address, LCD_CMD_DISPLAY_CONTROL | state->display_control);
    delayMicroseconds(50);
    
    return ERR_OK;
}

error_code_t lcd_1602_blink_enable(lcd_1602_state_t* state, bool enable) {
    if (!state || !state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    if (enable) {
        state->display_control |= LCD_BLINK_ON;
    } else {
        state->display_control &= ~LCD_BLINK_ON;
    }
    
    lcd_1602_send_command(state->i2c_address, LCD_CMD_DISPLAY_CONTROL | state->display_control);
    delayMicroseconds(50);
    
    return ERR_OK;
}

error_code_t lcd_1602_backlight_enable(lcd_1602_state_t* state, bool enable) {
    if (!state || !state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    state->backlight_state = enable ? 1 : 0;
    
    // Backlight state is handled in send_byte via LCD_BACKLIGHT flag
    // No command needed for PCF8574 backpack
    
    return ERR_OK;
}

error_code_t lcd_1602_print(lcd_1602_state_t* state, const char* str) {
    if (!state || !state->initialized || !str) {
        return ERR_INVALID_ARG;
    }
    
    while (*str) {
        lcd_1602_send_data(state->i2c_address, (uint8_t)*str);
        str++;
        
        // Update cursor position
        state->col++;
        if (state->col >= LCD_1602_COLS) {
            state->col = 0;
            state->row++;
            if (state->row >= LCD_1602_ROWS) {
                state->row = 0;
            }
            lcd_1602_set_cursor(state, state->col, state->row);
        }
    }
    
    return ERR_OK;
}

error_code_t lcd_1602_print_at(lcd_1602_state_t* state, uint8_t col, uint8_t row, const char* str) {
    if (!state || !state->initialized || !str) {
        return ERR_INVALID_ARG;
    }
    
    error_code_t err = lcd_1602_set_cursor(state, col, row);
    if (err != ERR_OK) {
        return err;
    }
    
    return lcd_1602_print(state, str);
}

error_code_t lcd_1602_print_aligned(lcd_1602_state_t* state, uint8_t row, const char* str, lcd_alignment_t alignment) {
    if (!state || !state->initialized || !str) {
        return ERR_INVALID_ARG;
    }
    
    // Calculate string length
    uint8_t len = 0;
    const char* p = str;
    while (*p && len < LCD_1602_COLS) {
        len++;
        p++;
    }
    
    // Calculate starting column based on alignment
    uint8_t col = 0;
    switch (alignment) {
        case LCD_ALIGN_LEFT:
            col = 0;
            break;
        case LCD_ALIGN_CENTER:
            col = (LCD_1602_COLS - len) / 2;
            break;
        case LCD_ALIGN_RIGHT:
            col = LCD_1602_COLS - len;
            break;
    }
    
    return lcd_1602_print_at(state, col, row, str);
}

error_code_t lcd_1602_print_int(lcd_1602_state_t* state, int value) {
    if (!state || !state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    char buffer[12];  // Enough for -2147483648
    snprintf(buffer, sizeof(buffer), "%d", value);
    return lcd_1602_print(state, buffer);
}

error_code_t lcd_1602_print_float(lcd_1602_state_t* state, float value, uint8_t decimals) {
    if (!state || !state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    char buffer[17];  // Max 16 chars + null
    snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    return lcd_1602_print(state, buffer);
}

error_code_t lcd_1602_create_char(lcd_1602_state_t* state, uint8_t slot, const uint8_t* bitmap) {
    if (!state || !state->initialized || !bitmap || slot >= LCD_CGRAM_SIZE) {
        return ERR_INVALID_ARG;
    }
    
    // Set CGRAM address
    lcd_1602_send_command(state->i2c_address, LCD_CMD_SET_CGRAM_ADDR | (slot << 3));
    delayMicroseconds(50);
    
    // Write 8 bytes of character data
    for (uint8_t i = 0; i < 8; i++) {
        lcd_1602_send_data(state->i2c_address, bitmap[i]);
    }
    
    // Store in state
    state->custom_chars[slot] = slot;
    
    // Return to DDRAM
    lcd_1602_set_cursor(state, state->col, state->row);
    
    return ERR_OK;
}

error_code_t lcd_1602_scroll_left(lcd_1602_state_t* state) {
    if (!state || !state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    lcd_1602_send_command(state->i2c_address, 0x18);  // Shift left
    delayMicroseconds(50);
    
    return ERR_OK;
}

error_code_t lcd_1602_scroll_right(lcd_1602_state_t* state) {
    if (!state || !state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    lcd_1602_send_command(state->i2c_address, 0x1C);  // Shift right
    delayMicroseconds(50);
    
    return ERR_OK;
}

error_code_t lcd_1602_stop_scroll(lcd_1602_state_t* state) {
    if (!state || !state->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    // Stop scrolling by setting entry mode to no-shift
    state->display_mode = LCD_ENTRY_INCREMENT | LCD_ENTRY_SHIFT_OFF;
    lcd_1602_send_command(state->i2c_address, LCD_CMD_ENTRY_MODE | state->display_mode);
    delayMicroseconds(50);
    
    return ERR_OK;
}

error_code_t lcd_1602_auto_detect_address(lcd_1602_state_t* state) {
    if (!state) {
        return ERR_INVALID_ARG;
    }
    
    // Try common I2C addresses for LCD modules
    uint8_t addresses[] = {LCD_1602_DEFAULT_ADDRESS, LCD_1602_ALTERNATE_ADDRESS};
    
    for (size_t i = 0; i < sizeof(addresses) / sizeof(addresses[0]); i++) {
        Wire.beginTransmission(addresses[i]);
        if (Wire.endTransmission() == 0) {
            state->i2c_address = addresses[i];
            state->initialized = true;
            return ERR_OK;
        }
    }
    
    return ERR_NOT_FOUND;
}

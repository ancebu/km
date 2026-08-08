# ESP32 Battery Monitor with Muxed Thermistor Matrix

A maintainable, modular ESP32 firmware for monitoring battery packs using a multiplexed thermistor matrix array. All sense wires (voltage, current, temperature) are routed through multiplexers directly to an INA226 current/voltage monitor for comprehensive data logging.

## Features

- **Muxed Thermistor Matrix**: Scan multiple temperature points across battery cells
- **Dual Multiplexer Architecture**: Route any sense wire to INA226 VBUS input
- **Comprehensive Logging**: Capture voltage, current, power, and temperature data
- **I2C LCD 1602 Display**: Real-time monitoring on 16x2 character display
- **Zero Official ESP-IDF Overhead**: Uses PlatformIO for easy setup
- **Modular Design**: Clean separation of concerns for long-term maintainability

## Quick Start for New Developers

### Prerequisites
- [PlatformIO Core](https://platformio.org/install/cli) (lightweight, no official ESP nonsense)
  ```bash
  pip install platformio
  ```
- USB cable for your ESP32 dev board

### Setup & Build
```bash
# Clone the repository
git clone <repo-url>
cd esp32-battery-monitor

# Build the project
pio run

# Upload to device
pio run --target upload

# Open serial monitor
pio device monitor
```

### Run Examples
```bash
# Basic sensing demo
pio run -e basic

# Advanced matrix scanning
pio run -e advanced
```

## Project Structure

```
esp32-battery-monitor/
├── src/                    # Main source files
│   ├── main.cpp           # Entry point
│   ├── drivers/           # Hardware abstraction layer
│   │   ├── gpio_driver.*  # GPIO management
│   │   ├── i2c_driver.*   # I2C communication
│   │   └── spi_driver.*   # SPI (if needed)
│   ├── mux/               # Multiplexer control logic
│   │   ├── mux_controller.*  # Dual mux coordination
│   │   └── mux_config.*      # Pin mappings & configurations
│   ├── ina226/            # INA226 sensor driver
│   │   ├── ina226_driver.*   # Register access & calibration
│   │   └── ina226_data.*     # Data structures & conversions
│   ├── thermistor/        # Temperature sensing
│   │   ├── thermistor_calc.* # Beta parameter calculations
│   │   └── temp_matrix.*     # Matrix scanning logic
│   ├── battery/           # Battery management
│   │   ├── cell_monitor.*    # Individual cell tracking
│   │   └── pack_state.*      # Overall pack health
│   └── sensors/           # Unified sensor interface
│       └── sensor_hub.*      # Aggregates all sensor data
├── include/               # Public headers
│   ├── types.h            # Common type definitions
│   ├── config.h           # Compile-time configuration
│   └── error_codes.h      # Error handling codes
├── lib/                   # Third-party libraries (if any)
├── tests/                 # Unit tests
│   ├── test_mux.cpp
│   ├── test_ina226.cpp
│   └── test_thermistor.cpp
├── examples/              # Standalone example sketches
│   ├── basic/             # Simple single-point measurement
│   └── advanced/          # Full matrix scanning demo
├── config/                # Configuration files
│   ├── default_config.h   # Default pinouts & settings
│   └── board_profiles/    # Board-specific configurations
├── docs/                  # Documentation
│   ├── hardware/          # Schematics, wiring diagrams
│   └── api/               # API reference
├── scripts/               # Utility scripts
│   ├── hardware/          # PCB gerber viewers, BOM generators
│   └── setup/             # Development environment setup
├── platformio.ini         # PlatformIO configuration
└── README.md
```

## Hardware Architecture

### Dual Multiplexer Design
- **MUX A**: Routes positive sense lines to INA226 VBUS+
- **MUX B**: Routes negative sense lines to INA226 VBUS-
- Enables direct measurement of any cell voltage or thermistor resistance

### Supported Components
- **MCU**: ESP32-WROOM, ESP32-S3, ESP32-C3
- **Current/Voltage Sensor**: INA226 (I2C)
- **Display**: HD44780-based 16x2 LCD with PCF8574 I2C backpack
- **Multiplexers**: CD74HC4051, CD74HC4052, or similar
- **Thermistors**: NTC 10kΩ (beta=3950), configurable

## Configuration

Edit `include/config.h` to customize:
- GPIO pin assignments
- I2C bus configuration
- Multiplexer channel count
- Thermistor beta values
- Sampling rates
- INA226 calibration constants
- LCD display settings (enable/disable, I2C address)

### LCD Configuration

To enable the I2C LCD 1602 display:

1. Set `CONFIG_LCD_ENABLED` to `1` in `include/config.h`
2. Connect your LCD module:
   - VCC → 5V (or 3.3V depending on your module)
   - GND → GND
   - SDA → GPIO 21 (or configured I2C SDA pin)
   - SCL → GPIO 22 (or configured I2C SCL pin)
3. The driver will auto-detect the I2C address (0x27 or 0x3F)

The display shows:
- Line 1: Voltage and Current (e.g., "V:12.34V C:1.23A")
- Line 2: Temperature and Power (e.g., "T:25.0C P:15.2W")

## Testing Strategy

### Unit Tests
Run all tests:
```bash
pio test
```

Run specific test:
```bash
pio test -f test_mux
```

### Hardware-in-the-Loop
Connect your ESP32 and run:
```bash
pio run --target upload
pio device monitor
```

## Development Best Practices

1. **No Global State**: Use dependency injection for hardware objects
2. **Hardware Abstraction**: All HW access through driver interfaces
3. **Error Handling**: Return error codes, never silently fail
4. **Logging**: Use ESP_LOG* macros with appropriate levels
5. **Configuration**: Compile-time options in `config.h`, runtime in NVS

## Troubleshooting

### Common Issues
- **INA226 not found**: Check I2C pull-up resistors (4.7kΩ recommended)
- **Mux switching noise**: Add 100ms settling delay after channel change
- **Temperature drift**: Recalibrate thermistor beta value for your range

### Debug Mode
Enable verbose logging in `config.h`:
```cpp
#define CONFIG_LOG_LEVEL DEBUG
```

## License

MIT License - See LICENSE file for details

## Contributing

1. Fork the repository
2. Create a feature branch
3. Write tests for new functionality
4. Submit a pull request

---

**Built with ❤️ using PlatformIO - No ESP-IDF installation required!**

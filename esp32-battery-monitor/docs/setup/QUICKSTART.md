# Quick Start Guide for New Developers

## 5-Minute Setup

### Step 1: Install PlatformIO (No ESP-IDF!)
```bash
pip install platformio
```

That's it! PlatformIO automatically downloads the ESP32 toolchain on first build.

### Step 2: Clone and Build
```bash
git clone <repository-url>
cd esp32-battery-monitor
pio run
```

### Step 3: Upload to Device
```bash
pio run --target upload
```

### Step 4: Monitor Serial Output
```bash
pio device monitor
```

Press `Ctrl+]` to exit the monitor.

## Project Organization

```
esp32-battery-monitor/
├── src/              # Your code goes here
│   ├── main.cpp     # Entry point (like setup()/loop())
│   ├── mux/         # Multiplexer control
│   ├── ina226/      # Current sensor driver
│   └── thermistor/  # Temperature calculations
├── include/          # Header files
│   ├── config.h     # ← Edit this for pin assignments!
│   ├── types.h      # Data structures
│   └── error_codes.h
├── examples/         # Standalone examples
├── tests/            # Unit tests
└── platformio.ini    # Build configuration
```

## Common Tasks

### Change Pin Assignments
Edit `include/config.h`:
```cpp
#define CONFIG_I2C_SDA_PIN GPIO_NUM_21
#define CONFIG_MUX_A_SELECT_0 GPIO_NUM_18
// ... etc
```

### Add a New Sensor Module
1. Create `src/sensors/my_sensor.h` and `my_sensor.cpp`
2. Include in `main.cpp`
3. Initialize in `initialize_system()`

### Run Tests
```bash
pio test
```

### Build for Different Boards
```bash
# ESP32-WROOM (default)
pio run -e esp32dev

# ESP32-S3
pio run -e esp32s3

# ESP32-C3
pio run -e esp32c3

# Debug build with verbose logging
pio run -e debug
```

### Clean Build Artifacts
```bash
pio run --target clean
```

## Development Workflow

### 1. Make Changes
Edit source files in `src/` or headers in `include/`

### 2. Build
```bash
pio run
```

### 3. Upload
```bash
pio run --target upload
```

### 4. Monitor
```bash
pio device monitor
```

### 5. Test
```bash
pio test
```

## Understanding the Code

### Main Loop Flow
```
setup() → initialize_system()
   ↓
loop() → scan_thermistor_matrix()
       → update_pack_state()
       → log_pack_state()
```

### Key Components

**Mux Controller** (`src/mux/`)
- Controls two 8-channel multiplexers
- Routes any sense wire pair to INA226
- Handles channel switching and settling delays

**INA226 Driver** (`src/ina226/`)
- I2C communication with current sensor
- Calibration for accurate measurements
- Reads voltage, current, power

**Thermistor Math** (`src/thermistor/`)
- Beta equation for temperature calculation
- Voltage divider resistance calculation
- Calibration offset/gain support

## Debugging Tips

### Enable Verbose Logging
Build with debug environment:
```bash
pio run -e debug
```

Or edit `include/config.h`:
```cpp
#define CONFIG_LOG_LEVEL CONFIG_LOG_LEVEL_DEBUG
```

### Common Issues

**"INA226 not found"**
- Check I2C wiring (SDA/SCL)
- Verify pull-up resistors (4.7kΩ)
- Try different I2C address (0x40, 0x41, 0x44, 0x45)

**Compilation errors**
- Run `pio run --target clean`
- Delete `.pioenvs/` folder
- Reinstall: `pio run`

**Upload fails**
- Hold BOOT button while pressing EN/RST
- Check USB cable (some are charge-only)
- Try different USB port

## Learning Resources

- **PlatformIO Docs**: https://docs.platformio.org
- **ESP32 Arduino Core**: https://github.com/espressif/arduino-esp32
- **INA226 Datasheet**: Texas Instruments
- **CD74HC4051 Datasheet**: Texas Instruments

## Getting Help

1. Check existing issues on GitHub
2. Review `docs/hardware/SETUP.md` for wiring
3. Examine `examples/` for working code
4. Enable debug logging and check serial output

## Next Steps

Once you have the basic setup working:

1. ✅ Customize pin assignments in `config.h`
2. ✅ Wire your hardware per `docs/hardware/SETUP.md`
3. ✅ Test with `examples/basic/` sketch
4. 📚 Study the full matrix scanning in `main.cpp`
5. 🔧 Add custom features (data logging, WiFi, etc.)
6. 🧪 Write unit tests for new functionality

---

**Remember**: This project uses **PlatformIO only** - no need to install the massive ESP-IDF framework! PlatformIO handles all toolchain dependencies automatically.

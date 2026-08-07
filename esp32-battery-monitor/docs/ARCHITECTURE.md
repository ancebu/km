# Architecture Decision Record: ESP32 Battery Monitor

## Overview

This document describes the architectural decisions made for the ESP32 Battery Monitor project to ensure long-term maintainability and ease of onboarding for new developers.

## Core Decisions

### 1. PlatformIO Over Official ESP-IDF

**Decision**: Use PlatformIO with Arduino framework instead of official ESP-IDF.

**Rationale**:
- **Zero overhead setup**: `pip install platformio` vs 2GB+ ESP-IDF installation
- **Automatic toolchain management**: PlatformIO downloads compilers as needed
- **Multi-board support**: Single codebase compiles for ESP32, S3, C3 variants
- **Native testing**: Can run unit tests on host machine without hardware
- **Industry adoption**: Widely used in professional embedded development

**Trade-offs**:
- Slightly larger binary than bare ESP-IDF (Arduino abstraction layer)
- Less fine-grained control over low-level peripherals

### 2. Modular Directory Structure

**Decision**: Organize code by functional domain, not file type.

```
src/
├── mux/          # Multiplexer control logic
├── ina226/       # Current sensor driver
├── thermistor/   # Temperature calculations
├── battery/      # Battery state management
└── sensors/      # Unified sensor interface
```

**Rationale**:
- **Discoverability**: Easy to find code related to specific hardware
- **Encapsulation**: Each module has clear boundaries
- **Testability**: Modules can be tested independently
- **Reusability**: Drivers can be extracted for other projects

**Trade-offs**:
- More directories than traditional src/include split
- Requires discipline to maintain module boundaries

### 3. Configuration via Header Files

**Decision**: Use compile-time configuration in `include/config.h` instead of runtime menus.

**Rationale**:
- **Type safety**: Compiler catches invalid configurations
- **No flash wear**: No NVS writes for static settings
- **Dead code elimination**: Unused features compiled out
- **Documentation**: Config file serves as hardware specification
- **Version control**: Configuration changes tracked in git

**Trade-offs**:
- Requires recompilation to change settings
- Less flexible than runtime configuration

### 4. Explicit Error Handling

**Decision**: Return error codes instead of using exceptions or silent failures.

**Rationale**:
- **Predictable**: No unexpected stack unwinding on embedded systems
- **Memory efficient**: No exception handling overhead
- **Clear contracts**: Function signatures document failure modes
- **Testable**: Errors can be systematically tested

```cpp
error_code_t mux_controller_init(mux_controller_t* controller);
```

**Trade-offs**:
- More verbose than exceptions
- Caller must remember to check return values

### 5. Hardware Abstraction Layer (HAL)

**Decision**: Separate hardware access from business logic.

**Rationale**:
- **Testability**: Hardware can be mocked for unit tests
- **Portability**: Easier to migrate to different MCU
- **Maintainability**: Hardware changes isolated to driver files
- **Parallel development**: Hardware and logic teams can work independently

**Example**:
```cpp
// Driver layer
error_code_t ina226_driver_read(ina226_driver_t*, ina226_data_t*);

// Business logic uses driver
void update_pack_state() {
    ina226_driver_read(&g_ina226, &data);
    // ... process data
}
```

### 6. Dual Multiplexer Architecture

**Decision**: Use two independent multiplexers (MUX A + MUX B) instead of single mux or dedicated ICs.

**Rationale**:
- **Flexibility**: Any sense wire pair can be routed to INA226
- **Cost effective**: Generic muxes vs expensive battery monitor ICs
- **Scalability**: 8x8 = 64 measurement points with only 6 GPIO pins
- **Redundancy**: If one mux fails, partial functionality remains

**Trade-offs**:
- More complex switching logic
- Settling delay required after channel changes
- Higher component count than integrated solutions

### 7. Ring Buffer for Data Logging

**Decision**: Implement circular buffer for sensor data storage.

**Rationale**:
- **Memory efficiency**: Fixed memory footprint
- **Real-time safe**: No dynamic allocation during operation
- **Data retention**: Keeps most recent N samples
- **Simple API**: Push/pop interface

**Trade-offs**:
- Limited history (CONFIG_RING_BUFFER_SIZE entries)
- Oldest data lost when buffer fills

### 8. Comprehensive Type System

**Decision**: Define explicit structs for all data types.

**Rationale**:
- **Self-documenting**: Clear what data each function expects/returns
- **Type safety**: Compiler catches mismatches
- **Extensibility**: Easy to add fields without breaking API
- **Serialization**: Structs map directly to wire formats

```cpp
typedef struct {
    float voltage_v;
    float current_a;
    float power_w;
    uint32_t timestamp_ms;
} ina226_data_t;
```

### 9. Board Profiles via Conditional Compilation

**Decision**: Support multiple board types through preprocessor defines.

**Rationale**:
- **Single codebase**: One repository for all board variants
- **Compile-time selection**: No runtime detection overhead
- **Customization**: Board-specific pinouts without code changes

```bash
pio run -e esp32dev
pio run -e esp32s3
pio run -e esp32c3
```

### 10. Test Pyramid Implementation

**Decision**: Implement tests at multiple levels.

```
        /\
       /  \      Manual Testing (hardware)
      /----\
     /      \    Integration Tests
    /--------\
   /          \  Unit Tests (native)
  /------------\
```

**Rationale**:
- **Fast feedback**: Unit tests run in milliseconds
- **Confidence**: Multiple test layers catch different issues
- **Documentation**: Tests show expected behavior
- **Regression prevention**: Automated checking

**Trade-offs**:
- Initial time investment in test infrastructure
- Hardware tests require physical devices

## Maintenance Guidelines

### Adding New Features

1. Create new module in appropriate `src/` subdirectory
2. Add header to `include/` if public API needed
3. Write unit tests in `tests/`
4. Update `config.h` if new configuration options needed
5. Document in `docs/`

### Modifying Existing Code

1. Check for dependent modules
2. Update error codes if behavior changes
3. Add migration notes to changelog
4. Ensure backward compatibility or bump major version

### Onboarding New Developers

1. Follow `docs/setup/QUICKSTART.md`
2. Run `pio run` to verify build
3. Study `examples/basic/` for simple use case
4. Review architecture decisions in this document

## Future Considerations

### Potential Improvements

- **OTA Updates**: Add wireless firmware update capability
- **CAN Bus**: Support for automotive communication
- **Bluetooth/BLE**: Wireless monitoring interface
- **SD Card Logging**: Extended data storage
- **Web Interface**: Built-in configuration portal

### Scalability Limits

- Current design supports up to 64 temperature sensors (8x8 matrix)
- For larger packs, consider daisy-chaining muxes or I2C GPIO expanders
- Ring buffer size should be tuned based on available RAM

## References

- PlatformIO Documentation: https://docs.platformio.org
- ESP32 Arduino Core: https://github.com/espressif/arduino-esp32
- INA226 Datasheet: Texas Instruments
- CD74HC4051 Datasheet: Texas Instruments

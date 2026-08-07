# Hardware Setup Guide

## Required Components

### Core Electronics
- **ESP32 Development Board** (any variant: WROOM, S3, C3)
- **INA226 Current/Voltage Sensor Module**
- **2x Multiplexers** (CD74HC4051 for 8-channel, or CD74HC4052 for differential)
- **NTC Thermistors** (10kΩ @ 25°C, Beta=3950 recommended)
- **Shunt Resistor** (2mΩ to 10mΩ, depending on current range)
- **Series Resistors** (10kΩ for thermistor voltage dividers)

### Optional
- I2C pull-up resistors (4.7kΩ if not on module)
- Decoupling capacitors (0.1µF near each IC)
- Fuse for overcurrent protection
- Connector headers for battery sense wires

## Wiring Diagram

### INA226 Connection
```
ESP32      INA226
-----      ------
GPIO 21 -> SDA
GPIO 22 -> SCL
3.3V    -> VCC
GND     -> GND
         -> VBUS+ (from MUX A common)
         -> VBUS- (from MUX B common)
```

### Multiplexer A (Positive Sense Lines)
```
ESP32      MUX A (CD74HC4051)
-----      ------------------
GPIO 18 -> S0 (Select 0)
GPIO 19 -> S1 (Select 1)
GPIO 23 -> S2 (Select 2)
GPIO 5  -> EN (Enable, active low)
         -> Z (Common output to INA226 VBUS+)
Y0-Y7   -> Cell positive sense wires
```

### Multiplexer B (Negative Sense Lines)
```
ESP32      MUX B (CD74HC4051)
-----      ------------------
GPIO 25 -> S0 (Select 0)
GPIO 26 -> S1 (Select 1)
GPIO 27 -> S2 (Select 2)
GPIO 4  -> EN (Enable, active low)
         -> Z (Common output to INA226 VBUS-)
Y0-Y7   -> Cell negative sense wires
```

### Thermistor Matrix Wiring
Each thermistor connects between:
- One end: MUX A channel output
- Other end: MUX B channel output

This creates an 8x8 = 64 point temperature sensing matrix.

### Power Supply
- ESP32: 3.3V regulated (use onboard regulator or external)
- Muxes: 3.3V to 5V (check datasheet)
- INA226: 2.7V to 5.5V

## Configuration

Edit `include/config.h` to match your wiring:

```cpp
// I2C pins
#define CONFIG_I2C_SDA_PIN GPIO_NUM_21
#define CONFIG_I2C_SCL_PIN GPIO_NUM_22

// MUX A pins
#define CONFIG_MUX_A_SELECT_0 GPIO_NUM_18
#define CONFIG_MUX_A_SELECT_1 GPIO_NUM_19
#define CONFIG_MUX_A_SELECT_2 GPIO_NUM_23
#define CONFIG_MUX_A_ENABLE   GPIO_NUM_5

// MUX B pins
#define CONFIG_MUX_B_SELECT_0 GPIO_NUM_25
#define CONFIG_MUX_B_SELECT_1 GPIO_NUM_26
#define CONFIG_MUX_B_SELECT_2 GPIO_NUM_27
#define CONFIG_MUX_B_ENABLE   GPIO_NUM_4

// INA226 settings
#define CONFIG_INA226_SHUNT_R 0.002f  // 2 milliohm
#define CONFIG_INA226_MAX_CURRENT 10.0f  // 10 amps
```

## Testing

### 1. Verify I2C Communication
Run the I2C scanner example to confirm INA226 is detected:
```bash
pio run -e basic
pio device monitor
```

### 2. Test Multiplexer Switching
Check that mux channels change correctly with a multimeter.

### 3. Calibrate Temperature Sensors
Place thermistors in known temperature environment and adjust beta value.

### 4. Validate Current Measurement
Compare INA226 readings with a calibrated multimeter.

## Safety Notes

⚠️ **Battery Safety**
- Always use appropriate fuses
- Ensure proper insulation for high-voltage packs
- Never work on live circuits without proper PPE
- Implement hardware disconnects for emergency shutdown

⚠️ **Current Sensing**
- Choose shunt resistor rated for maximum current
- Consider power dissipation: P = I²R
- Use Kelvin connections for accurate measurement

## Troubleshooting

| Issue | Possible Cause | Solution |
|-------|---------------|----------|
| INA226 not found | Wrong I2C address | Check address pins, try 0x40, 0x41, 0x44, 0x45 |
| No temperature readings | Mux enable pin wrong | Verify active-low logic |
| Inaccurate current | Wrong shunt value | Update CONFIG_INA226_SHUNT_R |
| Temperature drift | Beta mismatch | Measure and calibrate beta parameter |
| Mux switching noise | No settling delay | Increase CONFIG_MUX_SETTLING_MS |

## Next Steps

1. Review `examples/basic/` for simple testing
2. Study `examples/advanced/` for full matrix scanning
3. Customize `config/default_config.h` for your application
4. Implement data logging (SD card, WiFi, BLE)
5. Add balancing control outputs

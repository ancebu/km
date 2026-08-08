# Test Harness Fix & Technical Debt Remediation Plan

## Executive Summary

The "0 of 0 tests" problem has been fixed, but this revealed deeper structural issues where:
1. Only hand-duplicated `_native` stubs are testable
2. Real modules have Arduino dependencies preventing host testing
3. Documentation claims (cubic spline, 0.01-0.05°C precision) don't match implementation
4. Global singletons cause test state contamination
5. Stack buffer overflow vulnerability exists in polynomial fitting

---

## 1. Immediate Fix Applied ✅

**File:** `platformio.ini`

**Before:**
```ini
test_filter = tests/test_mux.cpp
```

**After:**
```ini
test_filter = test_mux.cpp
```

**Result:** Tests now compile and run (178 assertions pass), but only test coordinate math and ring buffer—none of the core calibration logic.

---

## 2. Structural Problems

### 2.1 Code Duplication Hazard

Three copies of thermistor calculation logic exist:
- `src/thermistor/thermistor_calc.cpp` — Real implementation (includes `<esp_log.h>`)
- `src/utils/thermistor_calc_native.cpp` — Native stub (used by tests)
- `tests/thermistor_calc_native.cpp` — Another native stub (duplicate of above)

Same pattern for `ring_buffer`:
- `src/utils/ring_buffer_native.cpp` 
- `tests/ring_buffer_native.cpp`

**Risk:** Bugs fixed in one copy won't propagate to others. Tests verify stale code.

### 2.2 Hardware Dependencies Block Testing

| Module | Arduino Dependency | Testable? |
|--------|-------------------|-----------|
| `thermistor_calc.cpp` | `<esp_log.h>` only | ✅ Yes (remove log) |
| `hoge_polynomial.cpp` | `<Arduino.h>` (millis) | ⚠️ Needs clock injection |
| `equilibrium_detector.cpp` | `<Arduino.h>` (millis) + global config | ⚠️ Needs both fixes |
| `lut_calibration.cpp` | `<Arduino.h>` (millis) + global spline cache | ⚠️ Needs both fixes |
| `mux_controller.cpp` | `<Arduino.h>`, `<esp_log.h>`, GPIO | ❌ Needs mock layer |
| `aht20_driver.cpp` | `<Wire.h>` | ❌ Needs I2C mock |
| `ina226_driver` | No `.cpp` shipped | ❌ Missing entirely |
| `mosfet_switch.cpp` | `<Arduino.h>` + global config | ❌ Needs mock + singleton fix |
| `lcd_1602_i2c.cpp` | `<Wire.h>` | ❌ Needs I2C mock |

### 2.3 Documentation vs. Code Mismatches

#### Claim 1: "Cubic Spline Interpolation" (lut_calibration.h line 3, PDF)
**Reality:** `lut_interp_temp()` uses plain linear interpolation (lines 205-217):
```cpp
// Linear interpolation between nearest neighbors
// (Simplified from full cubic spline for embedded efficiency)
float ratio = (resistance_ohm - r0) / (r1 - r0);
*temp_c_output = t0 + ratio * (t1 - t0);
```

`build_natural_spline()` exists but is **dead code**—never called.

#### Claim 2: "0.01°C to 0.05°C precision" (PDF, lut_calibration.h line 8)
**Reality:** Zero tests validate precision against ground truth. No test compares LUT/polynomial output to known temperature values.

#### Claim 3: "100% Overall Compliance" (IMPLEMENTATION_STATUS.md)
**Reality:** Same document's "Next Steps" section admits `main.cpp` never calls calibration framework. Self-contradictory.

### 2.4 Global Singleton Hazards

**File:** `src/equilibrium/equilibrium_detector.cpp` (lines 17-24)
```cpp
static equilibrium_config_t g_default_config = { ... };
```

**File:** `src/lut/lut_calibration.cpp` (lines 16-17)
```cpp
static spline_coefficient_t* g_spline_cache = NULL;
static uint16_t g_spline_cache_size = 0;
```

**File:** `src/mosfet_switch/mosfet_switch.cpp`
```cpp
static mosfet_config_t g_default_config = { ... };
```

**Impact:** Two `equilibrium_state_t` objects in the same process silently share one config. Tests calling `_init()` twice with different configs will leak state.

### 2.5 Stack Buffer Overflow Vulnerability

**File:** `src/polynomial/hoge_polynomial.cpp` (line 193)
```cpp
poly_calibration_sample_t all_samples[HOGE_REFIT_SAMPLE_THRESHOLD_POWER_OF_10 + 2];  // 502 elements
```

But `poly_fit_state_t.max_samples` is caller-supplied with no validation:
```cpp
error_code_t hoge_polynomial_init(poly_fit_state_t* state, uint32_t max_samples) {
    state->max_samples = max_samples;  // No upper bound check!
}
```

**Exploit:** Call `hoge_polynomial_init(state, 1000)` then add 501+ samples → stack corruption.

---

## 3. Remediation Plan (Ordered by ROI)

### Phase 1: Decouple Pure Logic (Week 1)

#### Task 1.1: Remove ESP-IDF Dependencies from Testable Modules
**Files:** `src/thermistor/thermistor_calc.cpp`

Replace:
```cpp
#include <esp_log.h>
static const char* TAG = "THERM_CALC";
ESP_LOGI(TAG, "...");
```

With conditional compilation:
```cpp
#ifdef ARDUINO_ARCH_ESP32
#include <esp_log.h>
#define LOG_INFO(fmt, ...) ESP_LOGI("THERM", fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...) /* noop */
#endif
```

#### Task 1.2: Inject Clock Dependency
**Files:** `src/equilibrium/equilibrium_detector.h/.cpp`, `src/polynomial/hoge_polynomial.h/.cpp`

Add to state struct:
```cpp
typedef uint32_t (*clock_fn_t)(void);

typedef struct {
    // ... existing fields ...
    clock_fn_t get_time_ms;  // Function pointer, defaults to millis()
} equilibrium_state_t;
```

Update API:
```cpp
error_code_t equilibrium_init(equilibrium_state_t* state, 
                              const equilibrium_config_t* config,
                              clock_fn_t clock_fn);  // New param
```

Default implementation for production:
```cpp
static uint32_t default_clock(void) { return millis(); }

error_code_t equilibrium_init(...) {
    state->get_time_ms = clock_fn ? clock_fn : default_clock;
}
```

#### Task 1.3: Eliminate Global Singletons
**Files:** `src/equilibrium/equilibrium_detector.cpp`, `src/lut/lut_calibration.cpp`, `src/mosfet_switch/mosfet_switch.cpp`

Move config into state struct:
```cpp
typedef struct {
    equilibrium_config_t config;  // Was: static g_default_config
    bool in_equilibrium;
    uint32_t stable_reading_count;
    // ... rest of state ...
} equilibrium_state_t;
```

Update all references from `g_default_config.field` to `state->config.field`.

#### Task 1.4: Delete Duplicate `_native` Files
Once real modules compile natively:
```bash
rm src/utils/thermistor_calc_native.cpp
rm src/utils/ring_buffer_native.cpp
rm tests/thermistor_calc_native.cpp
rm tests/ring_buffer_native.cpp  # Tests move to proper unit tests
```

Update `platformio.ini`:
```ini
build_src_filter = +<thermistor/*.cpp> +<polynomial/*.cpp> +<lut/*.cpp> +<equilibrium/*.cpp>
```

### Phase 2: Characterization Tests (Week 2)

#### Task 2.1: LUT Linear vs. Spline Test
**Goal:** Prove documentation claim is false (or fix code to match claim).

```cpp
TEST_CASE("LUT uses linear interpolation, not cubic spline", "[lut]") {
    lut_table_t lut;
    lut_init(&lut, 0, 256);
    
    // Add three colinear-but-curved points
    calibration_point_t p1 = {.aht20_temperature_c = 20.0f, .ntc_resistance_ohm = 15000.0f, .confidence_score = 1.0f};
    calibration_point_t p2 = {.aht20_temperature_c = 25.0f, .ntc_resistance_ohm = 12000.0f, .confidence_score = 1.0f};
    calibration_point_t p3 = {.aht20_temperature_c = 30.0f, .ntc_resistance_ohm = 9000.0f, .confidence_score = 1.0f};
    
    lut_add_point(&lut, &p1);
    lut_add_point(&lut, &p2);
    lut_add_point(&lut, &p3);
    
    float temp;
    // Interpolate at resistance halfway between p1 and p2
    lut_interp_temp(&lut, 13500.0f, &temp);
    
    // Linear interpolation would give exactly 22.5°C
    // Cubic spline would deviate based on curvature
    REQUIRE(temp == Approx(22.5f).epsilon(0.01));  // Will pass for linear, fail for spline
}
```

**Action based on result:**
- If test passes: Update docs to say "linear interpolation"
- If test fails: Either fix `lut_interp_temp()` to call spline code, or remove spline claims

#### Task 2.2: Polynomial/LUT Precision Test
```cpp
TEST_CASE("Polynomial fit achieves claimed precision", "[polynomial][precision]") {
    // Use NIST-traceable thermistor table as ground truth
    // Test that polynomial output matches within 0.05°C across calibrated range
}
```

#### Task 2.3: Stack Overflow Test
```cpp
TEST_CASE("hoge_polynomial rejects oversized sample counts", "[polynomial][safety]") {
    poly_fit_state_t state;
    error_code_t err = hoge_polynomial_init(&state, 1000);  // Exceeds stack array
    
    // Should return ERR_OUT_OF_RANGE or similar, NOT succeed
    REQUIRE(IS_ERROR(err));
}
```

#### Task 2.4: Equilibrium Hysteresis Test
```cpp
TEST_CASE("equilibrium_check exit hysteresis documented", "[equilibrium]") {
    // Pin down actual behavior: how many cycles after condition violation
    // before equilibrium exits?
}
```

### Phase 3: Hardware Mock Layer (Week 3+)

#### Task 3.1: Fake Wire Transaction Log
Create `mock_wire.h`:
```cpp
class MockWire {
public:
    void beginTransmission(uint8_t addr);
    size_t write(uint8_t data);
    uint8_t endTransmission();
    size_t requestFrom(uint8_t addr, size_t qty);
    uint8_t read();
    
    // Test interface
    void expect_write(uint8_t addr, const uint8_t* bytes, size_t len);
    void expect_read(uint8_t addr, const uint8_t* response, size_t len);
    void verify();  // Assert all expected transactions occurred
};
```

Then refactor drivers to accept `MockWire&` instead of global `Wire`.

---

## 4. Updated platformio.ini Recommendations

```ini
[env:native]
platform = native
build_flags = 
    -std=c++17
    -I./include
    -I./src
    -DARDUINO_ARCH_ESP32
    -include ./include/config.h
lib_deps = 
    baracodadailyhealthtech/catch2@^3.7.1
test_filter = test_mux.cpp
build_src_filter = 
    +<thermistor/*.cpp>
    +<polynomial/*.cpp>
    +<lut/*.cpp>
    +<equilibrium/*.cpp>
    -<*/ *_native.cpp>  ; Exclude old stubs once deleted
```

---

## 5. Documentation Corrections Required

| Document | Claim | Correction Needed |
|----------|-------|-------------------|
| `lut_calibration.h` line 3 | "Cubic Spline Interpolation" | → "Linear Interpolation" or implement spline |
| `lut_calibration.h` line 8 | "0.01°C to 0.05°C precision" | Add precision test or remove claim |
| `IMPLEMENTATION_STATUS.md` | "100% Overall Compliance" | → Mark calibration framework as untested until Phase 2 complete |
| PDF | Spline interpolation reaching 0.01-0.05°C | Update to reflect actual linear interpolation performance |

---

## 6. Success Criteria

- [ ] `pio test -e native` runs >100 meaningful test cases (not 0 of 0)
- [ ] No `_native` duplicate files exist
- [ ] All pure-logic modules compile without Arduino headers
- [ ] Global singletons eliminated from `equilibrium_detector`, `lut_calibration`, `mosfet_switch`
- [ ] Stack overflow test proves bounds checking works
- [ ] LUT interpolation type documented correctly (linear vs. spline)
- [ ] At least one precision test validates 0.01-0.05°C claim (or claim removed)

---

## Appendix: Current Test Output

```
Randomness seeded to: 4137960318
===============================================================================
All tests passed (178 assertions in 8 test cases)
```

Tests cover:
- Matrix coordinate conversion ✅
- Error code macros ✅
- Ring buffer push/pop ✅
- Thermistor default calibration ✅
- Temperature range validation ✅
- Fault flag operations ✅
- Configuration value sanity checks ✅

Tests **do not** cover:
- LUT interpolation accuracy ❌
- Polynomial fit precision ❌
- Equilibrium state machine transitions ❌
- Calibration point harvesting ❌
- Any driver-level I2C/GPIO operations ❌

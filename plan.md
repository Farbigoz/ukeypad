# ESP32-S3 USB HID Keypad — Development Plan

This document describes the planned evolution of the keypad firmware and
configurator. It is intentionally separate from [README.md](README.md):
`README.md` documents the current working firmware and quick start, while this
file tracks future architecture, decisions, milestones, and open questions.

The device is a general-purpose USB HID input controller. Rhythm games are one
possible use case, alongside shortcuts, productivity, accessibility, media
control, testing, and custom input workflows.

## Current baseline

Implemented today:

- ESP32-S3 SuperMini + Arduino framework + TinyUSB;
- six digital mechanical switches with `INPUT_PULLUP`;
- profile-defined hardware-timer scan (currently 2000 Hz);
- integrator debounce;
- standard USB HID Keyboard;
- simultaneous keys and duplicate logical bindings;
- CDC configuration mode selected by holding any key during boot;
- NVS-persisted key bindings;
- text CDC protocol: `bind`, `list`, `save`, `reset`, `info`, `test`,
  `debounce`, `stats`, `help`;
- standalone Web Serial GUI in [docs/configurator.html](docs/configurator.html);
- centralized compile-time profile in `src/DeviceProfile.h`;
- centralized firmware/protocol/storage versions in `src/FirmwareVersion.h`;
- separated `ConfigStorage`, `DeviceMetadata`, and `KeyNameTable` modules;
- HID state reset when USB stops/disconnects.

## Design principles

1. **Keep the latency-critical path small.** Input sampling, sensor processing,
   debouncing, event generation, and HID output must not depend on the GUI,
   NVS writes, serial output, RGB animation, or dynamic allocation.
2. **Separate hardware, input semantics, transport, and configuration.** A
   digital switch and a Hall sensor should produce the same high-level key
   events where possible, even though their sampling logic differs.
3. **Describe the device explicitly.** Firmware should expose a machine-readable
   capability/configuration descriptor so the GUI does not guess the number or
   type of inputs, LEDs, profiles, or supported settings.
4. **Prefer compile-time hardware configuration initially.** The first
   multi-hardware version should be deterministic and easy to audit. Runtime
   configuration should change bindings and behaviour, not unsafe GPIO wiring.
5. **The configured slot count is the physical device size.** A profile may
   contain 2, 6, 10, or another supported number of real buttons. There are no
   placeholder input slots in the device model; every slot represents a real
   physical button.
6. **One physical button has one corresponding LED.** The initial lighting
   model is one-to-one: LED index equals input slot index, and the configured
   LED count equals the configured button count.
7. **Preserve a safe fallback.** Invalid configuration must load defaults and
   must never configure USB pins, flash/PSRAM pins, or unsafe GPIOs as inputs.

## Roadmap overview

| Phase | Goal | Result |
|---|---|---|
| 0 | Stabilize the current firmware | Reliable baseline and regression checks |
| 1 | Introduce compile-time device description | One codebase supports different input layouts |
| 2 | Add Hall-effect input support | Digital and analog/Hall keys share the event pipeline |
| 3 | Support mixed input devices | A device may contain digital and Hall slots in any configured count |
| 4 | Expose capabilities/configuration over CDC | GUI can discover the device instead of hardcoding it |
| 5 | Build a dynamic configurator | GUI adapts to input types, counts, settings, and binds |
| 6 | Add configurable RGB lighting | Lighting hardware and controls are capability-driven |
| 7 | Verification and release hardening | Reproducible profiles, migration, diagnostics, documentation |
| 8 | Add validated MCU backends | Support only platforms with a suitable native USB/SDK stack |

---

## Phase 0 — Stabilize the current baseline

Do this before adding new sensor types. It prevents new architecture from being
built on undocumented assumptions.

### 0.1 Configuration format and versioning

- Add a versioned NVS record with `magic`, `version`, payload length, and CRC.
- Deliberately reject the old six-byte binding format; no migration is supported.
- Validate every loaded HID code and fall back to defaults on corruption.
- Make NVS write failures visible as `ERR code=NVS_WRITE_FAILED`.
- Keep debounce persistence separate from hardware description.

### 0.2 Protocol contract

- Document all CDC commands and response fields.
- Add `info` fields with stable names, for example:
  - `model`;
  - `firmware`;
  - `protocol`;
  - `config_version`;
  - `input_count`;
  - `input_types`;
  - `led_count`;
  - `capabilities`.
- Define one-line responses for machine parsing and a human-readable mode only
  if it is useful.
- Add a command to request the complete device description in one response.

### 0.3 Diagnostics and recovery

- Keep `test` on the same timer/debounce path as normal mode.
- Add explicit USB disconnect/reconnect state handling.
- Add queue overflow and maximum-depth counters.
- Add a full HID release/reset command for recovery.
- Add a boot-time validation window so a stuck key cannot accidentally make
  config mode difficult to leave.

### 0.4 Regression checklist

- Build with the installed PlatformIO/Arduino core.
- Test all six keys individually and simultaneously.
- Test duplicate bindings, including duplicate modifiers.
- Disconnect USB while keys are held and verify no stuck key after reconnect.
- Verify config mode never emits HID keypresses.
- Verify NVS save/load/reset and invalid-data fallback.

---

## Platform support policy

The project will not promise support for a microcontroller merely because an
Arduino core exists. A platform is supported only when it has a stable,
maintained USB device stack suitable for HID + CDC, a documented way to build
and flash with PlatformIO, and enough timer/GPIO/ADC/storage functionality for
the selected device profile.

### Preferred implementation layers

Use the platform's native, mature USB implementation:

- ESP32-S3: Arduino core/TinyUSB or the supported ESP-IDF USB device stack;
- STM32: STM32Cube USB Device/TinyUSB where the exact family support is solid;
- RP2040: Pico SDK TinyUSB, or a proven Arduino core only when it exposes the
  same USB capabilities without replacing the native stack;
- nRF52840: nRF5 SDK/ nrfx + TinyUSB or another established USB device stack;
- other STM32 families: evaluate independently; do not assume F0/F1/F4/G0/G4/
  H5/H7/U5 have identical USB, ADC, DMA, Flash, or bootloader capabilities.

Arduino may remain the application framework when it integrates the required
native stack cleanly. It is not a requirement. Implementing a USB HID device
from scratch in application code is not an acceptable portability strategy.

### Candidate platform tiers

**Tier 1 — primary targets**

- ESP32-S3: current reference implementation;
- STM32 families with suitable USB FS/HS support and a mature Cube/TinyUSB
  path, selected per board rather than as one generic STM32 target;
- RP2040/RP2350 when the chosen board exposes USB device and the Pico SDK
  integration is stable.

**Tier 2 — viable after a dedicated backend**

- nRF52840 and other nRF USB-capable parts;
- additional STM32 families whose USB, ADC/DMA, and nonvolatile storage need
  different implementations.

**Tier 3 — experimental / do not promise yet**

- CH32 families. Evaluate a specific part and SDK first. If USB HID + CDC,
  build/flash, and diagnostics are not reliable, leave CH32 unsupported rather
  than adding a fragile compatibility layer.

### Backend interface strategy

The latency-critical code should not use heap allocation, STL, or virtual
calls. A good first implementation is a small common C API with platform-owned
weak hook definitions, for example:

```cpp
// platform_api.h — common contract, no vendor headers
bool platform_input_begin(const DeviceDescription* device);
void platform_input_scan_or_process(void);
bool platform_input_pop_event(KeyEvent* event);
bool platform_hid_begin(void);
void platform_hid_press(uint8_t usage);
void platform_hid_release(uint8_t usage);
void platform_hid_release_all(void);
bool platform_storage_load(DeviceConfig* config);
bool platform_storage_save(const DeviceConfig* config);
```

Each backend provides the strong definitions for its platform. Weak defaults
may return `false` or compile-time errors for optional features, but must not
silently pretend that USB/ADC/storage is available.

Compile-time templates/type-erased static adapters are also valid when a
backend benefits from them:

```cpp
template<class InputBackend, class HidBackend, class StorageBackend>
class KeypadApplication { /* composition only; no virtual dispatch */ };
```

Use this only for composition and testability. Do not introduce ETL delegates
or a general callback framework until a real requirement appears; static
functions and a small C API are simpler for the current embedded scope.

Weak hooks are useful at the platform boundary, not for every internal event.
Core logic should call concrete, statically selected functions so the compiler
can inline latency-sensitive paths and detect missing backend symbols at link
time.

### Platform acceptance checklist

Before adding a platform to the supported list, verify on real hardware:

- USB HID keyboard enumerates reliably;
- USB CDC enumerates and works with the existing configurator;
- USB disconnect/reconnect clears HID state;
- PlatformIO build and upload are documented and repeatable;
- digital input scan meets the target frequency;
- timer/interrupt behaviour is measured, not assumed;
- ADC continuous/scan + DMA works if Hall profiles are claimed;
- configuration storage survives reset and power loss tests;
- safe GPIO/ADC restrictions are encoded in the device profile;
- memory and worst-case timing fit the smallest advertised board.

A platform that fails one of these is experimental until the specific gap is
fixed. The GUI and common protocol should not need platform-specific hacks.

---

## Phase 1 — Compile-time device description

This is the foundation for a universal device. It should be implemented before
Hall sensors or GUI generalization.

### 1.1 Replace hardcoded keypad constants

Introduce a central compile-time description, for example:

```cpp
// DeviceConfig.h
struct DigitalInputConfig {
    uint8_t gpio;
};

struct HallInputConfig {
    uint8_t adcChannel;
    uint8_t gpio;
};

enum class InputType : uint8_t {
    Digital,
    Hall
};

struct InputConfig {
    InputType type;
    uint8_t   pin;
    uint8_t   auxPin;
};

static constexpr InputConfig INPUTS[] = {
    { InputType::Digital, 4,  0 },
    { InputType::Digital, 5,  0 },
    { InputType::Hall,    1,  0 },
};
```

The exact representation should be decided after checking ESP32-S3 ADC
channel/pin constraints. The important point is that one table describes the
physical device at compile time.

### 1.2 Compile-time profiles

Start with named PlatformIO environments or a selected header:

- `device_digital_6` — current six MX-style switches;
- `device_digital_2` — minimal two-button keypad;
- `device_hall_2` — two Hall keys;
- `device_mixed_6` — digital + Hall inputs in one device.

Each profile should define:

- configured input count (for example 2, 6, or 10);
- input type for each physical slot;
- GPIO/ADC assignments;
- scan requirements;
- default bindings;
- LED configuration, when RGB exists.

Avoid copying source files between profiles. Only the description and selected
backend should differ.

### 1.3 Hardware safety validation

Add compile-time and startup validation for:

- duplicate GPIOs;
- GPIOs 19/20 reserved for native USB;
- flash/PSRAM pins;
- strapping pins;
- ADC-capable pins for Hall inputs;
- maximum input count for the HID report and event queue;
- incompatible ADC attenuation/resolution settings.

An invalid profile should fail the build when possible, not fail silently at
runtime.

### 1.4 Refactor input ownership

Refactor `Keypad` so it owns a fixed array of input backends or a tagged union,
not an array that assumes every input is a `Button`:

```text
Keypad
  ├── DigitalInput / Button
  ├── HallInput
  └── Event queue
```

The queue and HID layers should consume common events. They should not need to
know whether an event came from a digital or Hall sensor.

---

## Phase 2 — Hall-effect input support

Hall support should be added as a separate backend before mixing types.

### 2.1 Define the Hall sensor contract

Decide which sensor hardware is supported first:

- analog Hall sensor connected to an ESP32-S3 ADC pin; or
- digital Hall switch with a threshold output; or
- external I2C/SPI magnetic sensor.

The first implementation should target the simplest sensor that can meet the
sampling and latency requirements. Analog sensors require calibration and
stable ADC handling; digital Hall switches can reuse much of the digital
button path but do not provide Rapid Trigger depth information.

### 2.2 Hall sampling backend

Implement a `HallInput` class responsible for:

- ADC acquisition;
- optional oversampling/filtering;
- baseline and polarity;
- calibration state;
- actuation threshold;
- release threshold;
- hysteresis;
- sensor fault detection.

Do not put USB, NVS, or GUI logic into `HallInput`.

### 2.3 Calibration

Define a safe calibration flow:

```text
calibrate start <slot>
calibrate released
calibrate pressed
calibrate save
```

Or provide a GUI wizard. Calibration data should include:

- released value;
- pressed value;
- polarity;
- actuation threshold;
- release threshold;
- sensor range/quality.

Calibration must never run from the profile-defined scan ISR and must not write NVS for
every ADC sample.

### 2.4 Rapid Trigger as a separate feature

Do not couple basic Hall key detection and Rapid Trigger into one first step.
Implement in this order:

1. stable analog position reporting;
2. fixed actuation/release hysteresis;
3. configurable actuation depth;
4. configurable release depth;
5. Rapid Trigger based on movement direction and delta;
6. per-key tuning and diagnostic plots.

The regular event interface should remain `Press`/`Release` initially. A future
position event can be added only if the GUI or macro engine needs it.

### 2.5 Hall test and diagnostics

Extend config mode with:

```text
hall read <slot>
hall calibrate <slot>
hall status <slot>
hall stream <slot> on|off
```

The stream must be rate-limited and disabled by default so it cannot flood CDC
or disturb input timing.

---

## Phase 3 — Mixed digital and Hall inputs

Once each backend works separately, support a compile-time mixed table.

### 3.1 Common slot model

Every slot should expose metadata:

```text
slot index
input type: digital | hall
physical pin/channel
current binding
capabilities
calibration support
```

The slot array contains only real physical buttons. A two-button device has
`input_count=2`; it does not expose four disabled placeholder slots. A
10-button device has `input_count=10`, subject to the selected HID report and
queue limits.

### 3.2 Common event semantics

Both backends should produce the same high-level events:

```cpp
enum class KeyEventType { Press, Release };
struct KeyEvent {
    uint8_t slot;
    KeyEventType type;
    HidKeycode keyCode;
};
```

Adding `slot` is important for diagnostics, duplicate binding management, and
GUI test output. It avoids trying to infer the physical source from a keycode.

### 3.3 Scheduling

Start with one timer and one scan pass that visits every configured backend.
Only optimize to separate ADC/DMA tasks if measurements show the scan budget is
not sufficient. The scan path must have a documented worst-case execution time.

### 3.4 Input-specific settings

Keep common settings separate from backend-specific settings:

```text
common: binding, enabled, debounce
hall: calibration, actuation, release, rapid_trigger
led: brightness, color, effect
```

A digital slot should not expose Hall-only controls in the GUI.

---

## Phase 4 — Device description and protocol

The GUI must not hardcode six digital buttons forever. Firmware should expose a
machine-readable description.

### 4.1 Capability response

Add a command such as:

```text
get_device
```

Recommended response strategy:

- begin marker;
- one JSON-like or key-value line per field;
- end marker;
- explicit `OK`/`ERR` result.

Example conceptual response:

```text
OK device model=ukeypad-esp32-s3 firmware=0.2.0 protocol=2
OK inputs count=6 types=digital,digital,digital,digital,hall,hall
OK leds count=16 rgb=ws2812 brightness=true effects=true
OK capabilities binds=true test=true debounce=true calibration=true
OK device_end
```

A compact binary protocol can be introduced later if the text protocol becomes
too large. The first GUI should use the text protocol for debuggability.

### 4.2 Protocol versioning

The GUI should query the protocol version before rendering. Unknown fields must
be ignored; missing required fields should produce a clear error. Do not infer
hardware type from model strings.

### 4.3 Atomic configuration transactions

When configuration grows beyond one bind:

```text
config begin
config set input 2 bind F13
config set input 2 actuation 0.35
config set led brightness 80
config validate
config commit
```

`commit` should validate the complete configuration and write it once. This
avoids partially applied settings after a disconnect.

---

## Phase 5 — Universal dynamic GUI

Refactor [docs/configurator.html](docs/configurator.html) from a fixed six-row form into
a capability-driven UI.

### 5.1 Device discovery

On connect:

1. query protocol version;
2. query device description;
3. query current configuration;
4. build input cards from returned metadata;
5. show only controls supported by each slot;
6. render lighting controls only when LEDs are reported.

### 5.2 Input cards

Digital input card:

- slot number;
- GPIO label;
- binding selector;
- debounce control;
- test state;
- enabled/disabled state.

Hall input card:

- slot number;
- ADC/channel label;
- binding selector;
- live position indicator;
- calibration wizard;
- actuation threshold;
- release threshold;
- Rapid Trigger controls;
- sensor health/status.

The GUI renders exactly `input_count` physical slots. It does not render
placeholder rows for unpopulated buttons.

### 5.3 Configuration workflow

Use a local pending state in the browser:

```text
connect → read device/config → edit locally → validate → preview → commit/save
```

Do not send every slider movement directly to NVS. Debounce UI updates and
commit explicitly. Warn the user before rebooting or changing sensor
calibration.

### 5.4 Test and visualization

- show debounced press/release events per slot;
- show raw/filtered Hall position only when requested;
- show queue and scan diagnostics;
- display clear `OK`/`ERR code=...` messages;
- allow exporting/importing a configuration file on the host.

---

## Phase 6 — RGB lighting

RGB is intentionally postponed until the input/configuration model is stable.

### 6.1 Hardware abstraction

Create an `LedController` independent of input and HID layers. Initially target
WS2812/SK6812 through ESP32-S3 RMT, not timing-sensitive bit-banging.

Compile-time LED description should include:

- LED type;
- data GPIO;
- LED count;
- RGB vs RGBW;
- color order;
- maximum current policy.

### 6.2 Firmware capabilities

Expose:

```text
led info
led set brightness <0..100>
led set pixel <index> <r> <g> <b>
led effect <name>
led save
```

All LED updates must run outside the profile-defined input ISR. A low-priority task or
scheduled update is preferred.

### 6.3 GUI controls

Render lighting controls only if the device reports LED support:

- global brightness;
- per-key colors;
- idle/pressed/released colors;
- config/test/error colors;
- effect selection and speed;
- preview;
- save/apply/reset.

The GUI must warn about current draw and avoid high-frequency writes to NVS.

### 6.4 Interaction with input slots

The initial LED model is intentionally simple: LED index equals input slot
index, and `led_count == input_count`. The capability descriptor should still
report both counts explicitly so a later hardware revision can add status LEDs
or a different mapping without breaking protocol versioning.

---

## Phase 7 — Release hardening

Before calling the universal version stable:

- test every compile-time device profile;
- test digital-only, Hall-only, and mixed configurations;
- test 2-input, 6-input, 10-input, and maximum-supported profiles;
- validate USB disconnect while keys are held;
- validate NVS migration and corrupted data recovery;
- test GUI against each capability response;
- test config transactions interrupted during USB disconnect;
- measure scan worst-case execution time with all sensors enabled;
- document wiring, ADC restrictions, calibration, and safe GPIOs;
- keep a minimal HID-only build for environments where CDC is undesirable.

## Suggested next iteration order

The original requests are broad but fit naturally into the following small
iterations:

1. **NVS/protocol foundation**: version, CRC, `get_device`, stable errors.
2. **Compile-time device profiles**: digital 2/6/10-key profiles and mixed
   profiles with a configured physical slot count.
3. **Input abstraction refactor**: common slot metadata and `slot` in events.
4. **First Hall backend**: one sensor type, one Hall test command.
5. **Hall calibration**: released/pressed values and hysteresis.
6. **Mixed profile**: digital + Hall in one compile-time table.
7. **GUI discovery**: build digital-only UI dynamically from firmware data.
8. **GUI Hall controls**: calibration and live diagnostics.
9. **RGB abstraction and capability reporting**.
10. **GUI RGB controls and final integration tests**.
11. **Validated MCU backends**: RP2040/RP2350, selected STM32 families, then
    nRF52840 if the USB and storage acceptance checklist passes. Evaluate CH32
    separately; do not advertise it without a reliable native USB path.

This order keeps each change buildable and testable. It also avoids writing a
large GUI abstraction before the firmware has a stable device-description
protocol.

## Open decisions

These should be resolved before implementation of the corresponding phase:

- Which Hall sensor is the first supported hardware: analog voltage sensor,
  digital Hall switch, or external I2C/SPI sensor?
- Are Hall sensors connected directly to ESP32-S3 ADC pins, and which exact
  SuperMini pins are available on the target PCB?
- Is one timer scan pass sufficient for the maximum planned number of Hall
  channels, or is ADC continuous/DMA mode needed?
- Should the HID report remain six-key rollover, or should the universal
  profile support a larger custom report?
- Should the CDC protocol stay line-oriented text, or transition to framed JSON
  or a binary protocol after capability discovery is implemented?
- What exact RGB hardware is planned under each button (WS2812/SK6812 or
  another part), and what is the maximum LED current budget? The initial
  software model assumes one LED per physical button.
- Which settings are runtime-only and which are persisted in NVS?

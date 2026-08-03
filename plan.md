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

| Phase | Goal | Current status |
|---|---|---|
| 0 | Stabilize the current firmware | 0.1/0.2 complete; 0.3 and hardware checks pending |
| 1 | Introduce compile-time device description | Profile foundation complete; multiple profiles and slot-aware events pending |
| 2 | Add Hall-effect input support | Not started |
| 3 | Support mixed input devices | Not started |
| 4 | Expose capabilities/configuration over CDC | Firmware description complete; GUI consumption pending |
| 5 | Build a dynamic configurator | Not started |
| 6 | Add configurable RGB lighting | Not started |
| 7 | Verification and release hardening | Not started; hardware validation pending |
| 8 | Add validated MCU backends | Not started |

---

## Phase 0 — Stabilize the current baseline

Do this before adding new sensor types. It prevents new architecture from being
built on undocumented assumptions.

### 0.1 Configuration format and versioning — complete

Implemented in `src/ConfigStorage.*` and `src/FirmwareVersion.h`:

- versioned NVS record with `magic`, `version`, payload length, and CRC;
- deliberate rejection of the old six-byte binding format; no migration;
- HID-code validation with atomic fallback to profile defaults;
- `ERR code=NVS_WRITE_FAILED` on failed writes;
- debounce remains runtime-only and separate from the binding record.

### 0.2 Protocol contract — firmware foundation complete

Implemented in `src/DeviceMetadata.*`, `src/Config.*`, and the configurator:

- stable `info` fields: `model`, `firmware`, `protocol`, `config_version`,
  `input_count`, `input_types`, `led_count`, and `capabilities`;
- framed multiline `get_device` response with `device_begin`/`device_end`;
- deterministic `OK`/`ERR code=...` responses;
- explicit `OK list` terminator;
- protocol and storage behavior documented in `README.md`.

The configurator still uses a fixed six-slot layout. Dynamic descriptor-driven
rendering belongs to Phase 5.

### 0.3 Diagnostics and recovery — partially complete

Implemented:

- `test` on the same scan/debounce path;
- scan, event, overflow, and maximum queue-depth counters;
- HID release/reset on USB stop/disconnect;
- config mode consumes events without HID output.

Pending:

- explicit user-command full HID release/reset;
- boot-time validation window for a stuck key;
- dedicated USB reconnect state/diagnostics beyond the current stop callback.

### 0.4 Regression checklist — static checks complete, hardware pending

Completed in development:

- PlatformIO build;
- configurator JavaScript syntax check;
- configurator HTML parse check;
- diff/static review of the latency-critical path.

Still requires real hardware:

- individual, simultaneous, duplicate, and modifier bindings;
- USB disconnect while keys are held;
- config-mode HID suppression;
- NVS save/load/reset and corrupt-record fallback;
- measured scan frequency, debounce timing, and end-to-end HID latency.

Do not mark these hardware checks complete without testing the device.

---

## Current source-of-truth files

- `src/DeviceProfile.h` — model, USB mode, physical input slots, GPIOs,
  default bindings, scan frequency, timer derivation, debounce default, LEDs,
  capabilities, and compile-time profile validation.
- `src/FirmwareVersion.h` — firmware, CDC protocol, and NVS format versions.
- `src/ConfigStorage.*` — versioned NVS serialization, CRC, and validation.
- `src/DeviceMetadata.*` — machine-readable CDC output; not a configuration
  source.
- `src/KeyNameTable.*` — HID name parsing and reverse lookup.

The current profile is six digital inputs at 2000 Hz with a default debounce
threshold of four samples. Runtime debounce changes are temporary and are not
persisted.

---

## Phase 0 exit criteria

Phase 0 is considered firmware-complete when sections 0.1 and 0.2 remain
implemented, the pending 0.3 recovery commands are addressed, and the hardware
regression checklist has been executed on the reference device. Static build
checks alone do not constitute release validation.

---

## Phase 1 — Compile-time device description

### Current status

The profile foundation is implemented in `src/DeviceProfile.h`. Current code
uses profile data for GPIOs, default bindings, input count, scan frequency,
timer alarm derivation, debounce default, LED count, and capabilities. Safety
checks cover reserved/unsafe GPIOs and duplicate pins.

The phase is not complete because the project still has only one six-digital
profile, the standard HID path is limited to six simultaneous non-modifier
keys, and events do not yet carry a physical slot identifier.

### Remaining work

- add at least one additional digital profile, such as a two-input profile;
- define profile selection through PlatformIO environments or a selected header;
- keep each profile buildable without copying source files;
- add `slot` to `KeyEvent` and preserve it through the queue for diagnostics and
  future mixed input handling;
- decide whether profiles larger than six require a custom HID report before
  advertising ten-input support;
- keep profile validation and safe-GPIO checks compile-time where possible.

---

## Phase 2 — Hall-effect input support

Not started. First choose the supported sensor type and exact ESP32-S3 wiring
before implementing ADC, digital Hall, or I2C/SPI support.

---

## Phase 3 — Mixed digital and Hall inputs

Not started. Depends on Phase 2 and the slot-aware common event model from
Phase 1.

---

## Phase 4 — Device description and protocol

### Current status

The firmware-side description protocol is complete: `info`, `get_device`,
stable fields, framed multiline output, capabilities, GPIO metadata, and
protocol version are implemented and documented.

The remaining GUI work is tracked in Phase 5. Do not duplicate the firmware
profile in the GUI when dynamic discovery is implemented.

### Remaining work

- define how the GUI handles unsupported protocol versions;
- define required versus optional descriptor fields;
- add a current-configuration descriptor when transaction support is designed.

---

## Phase 5 — Universal dynamic GUI

Not started. Replace the fixed six-row form with descriptor-driven rendering
only after the device description contract and slot metadata are stable.

---

## Phase 6 — RGB lighting

Not started. The current profile reports zero LEDs and no RGB backend exists.

---

## Phase 7 — Release hardening

Not started. This phase must include real hardware validation, corrupted-record
recovery, USB disconnect tests, all supported profile builds, and GUI testing
against each descriptor. It must **not** require migration of the rejected old
six-byte NVS format; test that old records are rejected and defaults are used.

---

## Phase 8 — Additional MCU backends

Not started. Follow the platform acceptance policy below; do not advertise a
backend before real USB, timing, storage, and (where applicable) ADC/DMA tests.

---

## Legacy roadmap detail

The sections below retain design detail for Hall, mixed-input, GUI, RGB, and
backend work. Their status is governed by the phase status above; examples using
Hall inputs or LEDs are conceptual future examples, not current firmware output.

---

## Platform support policy

The project will not promise support for a microcontroller merely because an
Arduino core exists. A platform is supported only when it has a stable,
maintained USB device stack suitable for HID + CDC, a documented way to build and
flash with PlatformIO, and enough timer/GPIO/ADC/storage functionality for the
selected device profile.

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

Keep this interface as future design guidance; it is not implemented yet.

---

## Open decisions

Resolve these before implementing the corresponding phase:

- Which Hall sensor is the first supported hardware: analog voltage sensor,
  digital Hall switch, or external I2C/SPI sensor?
- Are Hall sensors connected directly to ESP32-S3 ADC pins, and which exact
  SuperMini pins are available on the target PCB?
- Is one timer scan pass sufficient for the maximum planned Hall channels, or is
  ADC continuous/DMA mode needed?
- Should profiles larger than six inputs use a custom HID report, or should the
  first multi-profile release remain limited to six-key rollover?
- Should the CDC protocol remain line-oriented text, or transition to framed
  JSON/binary only if capability discovery outgrows text?
- What exact RGB hardware is planned, and what is the maximum LED current budget?
- Which settings are runtime-only and which are persisted in NVS? Current
  bindings persist; debounce does not.

---

## Historical iteration order

The original implementation order covered NVS/protocol foundation, compile-time
profiles, input abstraction, Hall support, mixed profiles, GUI discovery, RGB,
and additional MCU backends. The NVS/protocol foundation and initial firmware
device-description work are now complete; this historical list is retained only
for context.

## Active next-iteration order

1. Complete Phase 0.3 recovery commands and document their behavior.
2. Add `slot` to `KeyEvent` and verify the current digital profile unchanged.
3. Add and build a second digital profile, likely a two-input profile.
4. Decide the HID report strategy for profiles larger than six inputs.
5. Select the first Hall sensor and implement its isolated backend.
6. Add calibration/hysteresis and then a mixed digital/Hall profile.
7. Make the configurator consume `get_device` dynamically.
8. Add RGB hardware abstraction only after input/profile capabilities are stable.
9. Harden supported profiles and perform real hardware acceptance tests.
10. Evaluate additional MCU backends only after the acceptance checklist passes.

This order reflects the actual repository state and avoids repeating completed
NVS/protocol work.

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
| 0 | Stabilize the current firmware | Firmware foundation complete; hardware acceptance checks pending |
| 1 | Introduce compile-time device description | Compile-time profile foundation complete; scalable HID support pending |
| 2 | Expand digital profiles and scalable HID | Required now; current boot report supports only up to six non-modifier keys |
| 3 | Build a descriptor-driven configurator | Not started |
| 4 | Expose capabilities/configuration over CDC | Firmware description complete; GUI consumption pending |
| 5 | Add configurable RGB lighting | Not started |
| 6 | Verification and release hardening | Not started; hardware validation pending |
| 7 | Add validated MCU backends | Not started |
| 8 | Analog/Hall inputs | Deferred; intentionally out of current scope |

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
rendering belongs to Phase 3.

### 0.3 Diagnostics and recovery — firmware foundation complete

Implemented:

- `test` on the same scan/debounce path;
- scan, event, overflow, and maximum queue-depth counters;
- HID release/reset on USB stop/disconnect;
- config mode consumes events without HID output.

The firmware does not attempt to infer whether a held button is intentionally
pressed or mechanically stuck. The existing config-mode boot gesture and the
`test` command are sufficient ways to exercise the physical inputs; no separate
stuck-key validation window is planned.

No manual HID-reset command is required at this time: HID state is reset by the
USB stop callback, and HID output is suppressed entirely in config mode.

### 0.4 Regression checklist — static checks complete, hardware pending

Completed in development:

- PlatformIO build;
- configurator JavaScript syntax check;
- configurator HTML parse check;
- diff/static review of the latency-critical path;
- implementation review of simultaneous keys and duplicate logical bindings;
- implementation review of integrator debounce and USB-stop HID cleanup.

Still requires real hardware if release-level acceptance evidence is needed:

- individual, simultaneous, duplicate, and modifier bindings;
- USB disconnect while keys are held, including reconnect behavior;
- config-mode HID suppression;
- NVS save/load/reset and corrupt-record fallback;
- measured scan frequency, debounce timing, and end-to-end HID latency.

These are hardware acceptance checks, not missing firmware features. Do not mark
them measured or complete without testing the device.

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

Phase 0 is firmware-complete: sections 0.1 and 0.2 are implemented, the 0.3
diagnostics and recovery foundation is in place, and the static checks pass.
Release validation additionally requires the hardware acceptance checklist on
the reference device. Static build checks alone do not constitute hardware
validation.

---

## Phase 1 — Compile-time device description

### Current status

The profile foundation is implemented in `src/DeviceProfile.h`. Current code
uses profile data for GPIOs, default bindings, input count, scan frequency,
timer alarm derivation, debounce default, LED count, and capabilities. Safety
checks cover reserved/unsafe GPIOs and duplicate pins.

The compile-time profile foundation is complete. A profile is defined in
`src/DeviceProfile.h`; its input count, GPIOs, input types, default bindings,
timer parameters, debounce default, LED metadata, capabilities, and safety
checks are consumed by the firmware without duplicating the current hardware
values elsewhere. Creating another digital profile is a supported future use of
this mechanism, but adding a second concrete profile is not required now.

The current HID adapter still uses the standard boot keyboard path, whose
six-key non-modifier rollover is an adapter limitation rather than a limitation
of the physical profile model. The six-button configuration in the current
profile is only one ready-to-use profile, not a firmware-wide six-button limit.
Scalable HID is therefore already required before treating larger digital
profiles as supported.

`KeyEvent` currently carries the logical HID code rather than the physical slot.
That is sufficient for the current HID path, including duplicate bindings. A
physical slot field may be added later if physical-input diagnostics or mixed
input handling requires it; it is not a Phase 1 blocker.

### Remaining work

- keep profile validation and safe-GPIO checks compile-time where possible;
- replace the current six-key boot keyboard limitation with a scalable HID
  report/backend so the ready-to-use six-button profile and future larger
  digital profiles are not constrained by the boot-report rollover ceiling;
- define profile selection/build integration when multiple concrete profiles
  are needed, without copying runtime source files.

---

## Phase 2 — Digital profile scaling and scalable HID

This phase replaces the previously planned Hall-input work in the active
roadmap. Analog/Hall buttons are explicitly deferred and are not required for
current releases.

The current six-button configuration is a ready-to-use profile, not the target
size of the firmware. The goal is to make the existing digital-button
architecture support profiles with six or more physical inputs without changing
the current profile semantics.

### Planned work

- add a second compile-time digital profile only when a concrete board requires
  it, without copying runtime source files;
- define profile selection through PlatformIO environments or a selected header;
- replace the six-key boot keyboard limitation with a scalable HID report/backend
  so profiles with six or more physical inputs can be supported without an
  artificial six-key rollover ceiling;
- keep profile validation, safe-GPIO checks, and input-count limits compile-time;
- preserve the current logical `KeyEvent` model unless physical-slot identity is
  required by a concrete feature.

---

## Phase 3 — Descriptor-driven configurator

The firmware-side device description protocol is already implemented. This phase
makes the Web Serial GUI consume that descriptor instead of duplicating the
currently selected compile-time profile.

### Planned work

- parse `get_device` and validate required descriptor fields;
- render the input list, names, types, bindings, and available capabilities from
  the device descriptor;
- handle unsupported protocol versions and missing optional fields explicitly;
- retain deterministic protocol error handling and a usable fallback message;
- keep the GUI compatible with the current digital-input profile.

---

## Phase 4 — Device description and protocol

The firmware-side description protocol is complete. This phase remains as the
contract-maintenance phase for descriptor evolution while the GUI work is
tracked in Phase 3.

### Current status

The firmware-side description protocol is complete: `info`, `get_device`,
stable fields, framed multiline output, capabilities, GPIO metadata, and
protocol version are implemented and documented.

The remaining GUI work is tracked in Phase 3. Do not duplicate the firmware
profile in the GUI when dynamic discovery is implemented.

### Remaining work

- define how the GUI handles unsupported protocol versions;
- define required versus optional descriptor fields;
- add a current-configuration descriptor when transaction support is designed.

---

## Phase 5 — Universal dynamic GUI

Superseded by Phase 3. Keep this heading as a historical roadmap reference;
the active descriptor-driven GUI work is tracked in Phase 3.

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

## Deferred analog-input scope

Analog/Hall inputs and mixed analog/digital profiles are intentionally deferred.
They are retained only as a possible future extension and are not part of the
active implementation sequence. No ADC, Hall calibration, hysteresis, DMA, or
analog-input GUI work should be started until this scope is explicitly reopened.

---

## Legacy roadmap detail

The sections below retain design detail for deferred analog inputs, RGB, GUI, and
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

- When analog/Hall inputs are eventually reopened, which sensor, wiring, and
  ADC/DMA sampling contract should be supported first?
- Which scalable HID report format should support profiles larger than six
  inputs while preserving ordinary keyboard compatibility?
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

1. Keep the current digital firmware stable and document its existing
   diagnostics and recovery behavior.
2. Implement a scalable HID report/backend now; the current six-button profile
   already establishes the need to support six or more physical inputs without
   the boot-report rollover ceiling.
3. Add and build a second compile-time digital profile when a concrete board
   requires it.
4. Make the configurator consume `get_device` dynamically.
5. Add RGB hardware abstraction only after digital input/profile capabilities are
   stable.
6. Harden supported profiles and perform real hardware acceptance tests.
7. Evaluate additional MCU backends only after the acceptance checklist passes.
8. Keep analog/Hall and mixed analog/digital input work deferred until explicitly
   reopened.

Creating additional compile-time profiles through `DeviceProfile.h` remains
supported, but no second profile is required by the current iteration. This
order reflects the actual repository state and avoids repeating completed
NVS/protocol/profile-foundation work.

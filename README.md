# ukeypad

A general-purpose USB HID keypad for games, shortcuts, macros, media control,
accessibility, testing, and custom input workflows. The current reference
profile targets six mechanical switches on an ESP32-S3 SuperMini and uses the
Arduino framework with the ESP32 core's TinyUSB stack.

The roadmap for compile-time profiles, Hall inputs, capability discovery, and
RGB output is documented in [plan.md](plan.md).

## Current hardware profile

The active profile is defined in [src/DeviceProfile.h](src/DeviceProfile.h).
Do not duplicate its values in firmware or documentation.

| Slot | GPIO | Default key | HID usage |
|---:|---:|---|---:|
| 0 | 4  | Z   | 0x1D |
| 1 | 5  | X   | 0x1B |
| 2 | 6  | C   | 0x06 |
| 3 | 7  | V   | 0x19 |
| 4 | 15 | F13 | 0x68 |
| 5 | 16 | F14 | 0x69 |

Each switch is wired between its GPIO and GND. Internal pull-ups are enabled;
pressed means the pin reads LOW.

## Architecture

```text
profile-defined hardware timer ISR
        |
        v
   Keypad::scan()       GPIO sampling + integrator debounce
        |               -> lock-free SPSC event queue
        v               -> binary semaphore
   main loop            drains events and forwards them to HID
        |
        v
   HidKeyboard          USBHIDKeyboard pressRaw/releaseRaw
```

- No `delay()` or slow polling loop in the latency-critical path.
- The timer frequency and alarm are derived from `DeviceProfile.h`.
- The current profile scans at 2000 Hz, equivalent to a 500 µs period.
- USB HID polling uses the standard 1 ms full-speed interval.
- The default debounce threshold is `DeviceProfile::DEFAULT_DEBOUNCE_SAMPLES`
  (currently 4), approximately 2 ms at the current scan rate.
- `debounce set N` changes the active threshold until reboot; debounce is not
  persisted in NVS.
- The current USB keyboard report supports six simultaneous non-modifier keys.

### Layering

| Layer | Knows about | Does not know about |
|---|---|---|
| `Button` | one GPIO and debounce | USB, HID, other inputs |
| `Keypad` | profile inputs, scan, queue, bindings, diagnostics | USB, HID |
| `HidKeyboard` | USB HID reports and duplicate-binding ownership | GPIO, debounce |
| `ConfigStorage` | versioned NVS binding records | CDC presentation |
| `DeviceMetadata` | machine-readable protocol output | NVS persistence |

## Browser configurator

[docs/configurator.html](docs/configurator.html) is a dependency-free Web Serial
GUI for the CDC configuration channel. It requires a current desktop Chrome or
Edge browser. Web Serial generally is not available in Firefox or Safari.

The page can be opened directly in Chrome/Edge or served locally:

```bash
python -m http.server 8000
```

Then open `http://localhost:8000/docs/` or
`http://localhost:8000/docs/configurator.html`.

### Usage

1. Unplug the keypad.
2. Hold any one button while connecting USB to enter config mode.
3. Open the configurator and click **Подключить CDC**.
4. Select the keypad's CDC COM port.
5. Change key selectors; each change sends a `bind <slot> <key>` command.
6. Click **Сохранить в NVS** to persist bindings.
7. Use `test`, `debounce`, `stats`, `info`, and `get_device` as needed.
8. Tap RESET without holding a key to return to normal keyboard mode.

Config mode uses the same profile-defined scan, debounce, and event queue as
normal mode. Events are consumed by `Config` and never sent as HID reports.

## Build and upload

Run commands from the repository root:

```bash
pio run
pio run -t upload
pio device monitor
```

The project uses the ESP32 legacy timer API exposed by the installed Arduino
core:

```cpp
timerBegin(timerNumber, divider, countUp);
timerAttachInterrupt(timer, callback, edge);
timerAlarmWrite(timer, alarmTicks, autoreload);
timerAlarmEnable(timer);
```

### Flashing the SuperMini

The board has no separate USB-UART bridge. To enter the ROM bootloader, hold
BOOT, tap RESET, then release BOOT. Run the upload command and tap RESET after
flashing. If PlatformIO cannot detect the port, pass `--upload-port COMx`.

The default build targets an ESP32-S3 DevKitC-1 board definition with 4 MB
flash and no PSRAM. The board definition is only a PlatformIO build target; the
active physical wiring is defined by `DeviceProfile.h`.

## USB settings

| Setting | Value | Meaning |
|---|---:|---|
| `ARDUINO_USB_MODE` | `0` | USB-OTG/TinyUSB, required for HID |
| `ARDUINO_USB_CDC_ON_BOOT` | `1` | Composite CDC + HID device |
| Upload mode | ROM download | BOOT + RESET procedure above |

## Configuring bindings

The device has two boot-selected modes:

| Mode | Entry | Scanning | CDC |
|---|---|---|---|
| Keyboard | Plug in normally | On, profile-defined | Inert |
| Config | Hold any switch while connecting | On, profile-defined | Commands accepted; HID suppressed |

Available commands:

```text
bind <slot> <key>
list
save
reset
info
get_device
test [on|off]
debounce [get|set N]
stats [clear]
help
```

Key names are case-insensitive. Supported groups include A–Z, 0–9, F1–F24,
ENTER, ESC, TAB, SPACE, editing/navigation keys, arrows, media keys, and
CTRL/SHIFT/ALT/GUI modifiers. See [src/KeyNameTable.cpp](src/KeyNameTable.cpp)
and [src/HidKeycode.h](src/HidKeycode.h).

Successful responses begin with `OK`; command errors begin with
`ERR code=...`. The `list` response ends with `OK list`.

### Device description

`info` returns one machine-readable line. `get_device` returns a framed,
multiline description:

```text
OK device_begin
OK device model=ukeypad-esp32-s3 firmware=0.2.0 protocol=1 config_version=1
OK inputs count=6 types=digital,digital,digital,digital,digital,digital pins=4,5,6,7,15,16
OK leds count=0
OK capabilities binds=true test=true debounce=true stats=true
OK device_end
```

Unknown fields should be ignored by clients.

### NVS storage format

Bindings are stored under namespace `ukeypad`, key `bindings`, as a 12-byte
versioned record:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | magic `0x4B50` (`KP`, little-endian) |
| 2 | 1 | record version from `FirmwareVersion.h` |
| 3 | 1 | payload length (`6` for the current profile) |
| 4 | 6 | one HID usage byte per slot |
| 10 | 2 | CRC-16/CCITT over bytes 0..9, little-endian |

The firmware deliberately does not read or migrate the old raw six-byte format.
Missing, truncated, unknown-version, corrupt, or invalid records leave the
compiled-in profile defaults active. A failed save returns
`ERR code=NVS_WRITE_FAILED`.

## Project files

```text
ukeypad/
├── platformio.ini
├── src/
│   ├── DeviceProfile.h       # compile-time hardware profile
│   ├── FirmwareVersion.h     # firmware/protocol/storage versions
│   ├── main.cpp              # timer, boot mode, event loop
│   ├── Button.h/.cpp         # one digital input and debounce
│   ├── Keypad.h/.cpp         # scan, queue, runtime bindings
│   ├── HidKeycode.h          # HID usage enum
│   ├── HidKeyboard.h/.cpp    # USB HID adapter
│   ├── Config.h/.cpp         # CDC orchestration and command dispatch
│   ├── ConfigStorage.h/.cpp  # versioned NVS records
│   ├── DeviceMetadata.h/.cpp # protocol output
│   └── KeyNameTable.h/.cpp   # HID name lookup
└── docs/
    ├── index.html
    └── configurator.html
```

## Measuring latency

There is no single latency number; measure each link separately:

1. **Scan rate:** scope a GPIO toggled by a diagnostic ISR or count interrupts.
   Expect `DeviceProfile::SCAN_FREQUENCY_HZ`.
2. **Debounce:** the default contribution is
   `DEFAULT_DEBOUNCE_SAMPLES / SCAN_FREQUENCY_HZ`; currently about 2 ms.
3. **Firmware to USB:** toggle a diagnostic GPIO when `HidKeyboard` handles an
   event and compare it with the physical edge.
4. **USB polling:** inspect the HID IN endpoint with a USB analyzer; full-speed
   polling is normally 1 ms.
5. **End to end:** timestamp a known fast input edge and host HID reception.

Hardware behavior has not been validated unless the device was actually tested.

## Future development

- Compile-time two-, six-, and ten-input profiles.
- Hall-effect input backend, calibration, hysteresis, and Rapid Trigger.
- Capability-driven configurator UI.
- RGB output through ESP32-S3 RMT, outside the scan ISR.
- Atomic configuration transactions.
- Validated additional MCU backends.

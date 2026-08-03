# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project scope

This is a PlatformIO Arduino firmware project for a general-purpose USB HID keypad on ESP32-S3 SuperMini. The current reference device has six mechanical switches:

- GPIO4 → Z
- GPIO5 → X
- GPIO6 → C
- GPIO7 → V
- GPIO15 → F13
- GPIO16 → F14

The longer-term roadmap is in [plan.md](plan.md): compile-time device profiles with variable button counts, digital/Hall mixed inputs, ADC+DMA Hall processing, capability-driven GUI, and one RGB LED per physical button.

## Common commands

Run commands from the repository root (`d:/work/projects/osu_keypad`):

```bash
# Build the current ESP32-S3 firmware
pio run

# Clean build artifacts, then rebuild
pio run -t clean
pio run

# Upload over the ESP32-S3 ROM bootloader
pio run -t upload

# Upload to a specified port
pio run -t upload --upload-port COMx

# Open the UART monitor (115200 baud)
pio device monitor

# Validate the embedded JavaScript in the web configurator
python -c "import re; s=open('docs/configurator.html',encoding='utf-8').read(); m=re.search(r'<script>(.*?)</script>',s,re.S); open('.configurator-check.js','w',encoding='utf-8').write(m.group(1))"
node --check .configurator-check.js
rm .configurator-check.js

# Basic HTML parser check
python -c "from html.parser import HTMLParser; HTMLParser().feed(open('docs/configurator.html',encoding='utf-8').read()); print('HTML parsed')"

# Check repository state
 git status --short --branch
```

There is currently no automated unit-test or lint suite. Firmware validation is presently the PlatformIO build plus hardware checks described in [README.md](README.md). Do not claim a hardware behavior has been verified unless the device was actually tested.

For flashing, if native USB is not enumerated as an upload port, hold **BOOT**, tap **RESET**, release **BOOT**, then run the upload command. The application uses USB-OTG/TinyUSB mode and composite CDC+HID (`ARDUINO_USB_MODE=0`, `ARDUINO_USB_CDC_ON_BOOT=1`).

## Repository layout

- `src/` — firmware.
- `docs/index.html` — GitHub Pages project landing page.
- `docs/configurator.html` — standalone Web Serial configuration GUI.
- `platformio.ini` — current ESP32-S3 PlatformIO environment.
- `README.md` — current firmware usage, wiring, config mode, protocol, and latency notes.
- `plan.md` — future architecture and multi-platform roadmap.

GitHub Pages is configured from `master` and `/docs`; keep web links relative within `docs/` so the published site works at `/ukeypad/`.

## Firmware architecture

The latency-critical path is:

```text
Timer ISR (2000 Hz)
  → Keypad::scan()
  → Button::update() for every input
  → integrator debounce
  → lock-free SPSC event queue
  → binary semaphore
  → main loop
  → HidKeyboard
  → USBHIDKeyboard pressRaw/releaseRaw
```

`Button` owns one physical input and only handles GPIO sampling/debounce. It must not know about USB. `Keypad` owns the fixed input array, binding table, scan pass, event queue, runtime binding access, debounce settings, and diagnostics; it must not know HID details. `HidKeyboard` owns USB reports and duplicate-binding reference counts. Duplicate physical inputs mapped to one HID usage are supported: the logical key is released only after the last physical owner releases it.

The event queue is single-producer/single-consumer: the timer ISR writes `_head`, while the main task reads `_tail`. Do not introduce blocking calls, Serial output, NVS writes, or USB calls into the scan ISR.

`HidKeyboard` registers an `ARDUINO_USB_STOPPED_EVENT` callback. On USB stop/disconnect it calls `releaseAll()` and clears duplicate-binding refcounts to prevent stuck keys after reconnect.

## Timer and ISR compatibility

The installed environment is PlatformIO Espressif 32 7.0.1 with Arduino ESP32 core package `3.20017.241212`, but this core exposes the **legacy Arduino timer API** in the current build. The code intentionally uses:

```cpp
timerBegin(timerNumber, divider, countUp);
timerAttachInterrupt(timer, callback, edge);
timerAlarmWrite(timer, alarmTicks, autoreload);
timerAlarmEnable(timer);
```

The current configuration uses timer 0, divider 80, and a 500-tick alarm: APB 80 MHz / 80 = 1 MHz, so 500 µs gives 2000 Hz. Do not blindly replace this with the newer `timerBegin(frequency)`/`timerAlarm()` API without checking the installed framework headers and rebuilding.

`IRAM_ATTR` is applied only to function definitions in `.cpp` files. Headers intentionally do not use it because some include paths parse them before `Arduino.h` defines the macro. GPIO reads currently use `digitalRead()` rather than version-dependent `GPIO.in`/`GPIO.in1` register layouts; preserve compilation compatibility unless a measured performance issue justifies a platform-specific fast GPIO backend.

## Normal mode and config mode

Both modes run the same 2000 Hz timer, GPIO scan, integrator debounce, and event queue. The only difference is the final consumer:

- **Normal mode:** events are forwarded to `HidKeyboard`; CDC command handling is inert.
- **Config mode:** hold any switch while booting; events are consumed by `Config::processKeyEvents()` and never reach HID. With `test on`, debounced events are printed over CDC.

The config protocol is line-oriented and case-insensitive. Current commands include:

```text
bind <slot> <key>
list
save
reset
info
test [on|off]
debounce [get|set N]
stats [clear]
help
```

Successful responses begin with `OK`; errors use `ERR code=...`. Keep protocol responses deterministic and machine-parseable because the browser GUI depends on them.

Bindings are stored in ESP32 NVS under namespace `osukp`. Current NVS storage is a six-byte binding payload; the roadmap calls for adding version/magic/CRC before expanding configuration formats. Debounce changes are currently runtime-only unless explicitly extended.

## Web configurator

`docs/configurator.html` uses Web Serial and requires a Chromium desktop browser (Chrome/Edge). It expects config mode and sends the text protocol above. Keep it dependency-free and relative-path compatible with GitHub Pages. When changing firmware protocol responses, update the GUI parser and README together.

The GUI currently assumes six slots and fixed key choices; future capability-driven GUI work must wait for a stable device-description protocol as outlined in `plan.md`.

## Platform expansion rules

The current implementation is ESP32-S3-specific. Future backends should keep the common core free of vendor headers and isolate platform code for USB, timers, GPIO, ADC/DMA, and storage.

Do not promise support for a platform merely because an Arduino core exists. A supported backend needs a mature native USB device stack for HID+CDC, reproducible PlatformIO build/upload, measured timer/input behavior, appropriate ADC+DMA support for Hall profiles, and reliable nonvolatile storage. Preferred directions are ESP32-S3 TinyUSB, selected STM32 families through Cube/TinyUSB, RP2040/RP2350 through Pico SDK/TinyUSB, and nRF52840 through an established USB stack. CH32 remains experimental until a specific part and USB path pass the acceptance checklist in [plan.md](plan.md).

A small common C API or statically selected template composition is preferred over virtual dispatch in the latency path. Weak hooks are appropriate at the platform boundary, not throughout the core. Avoid adding ETL delegates or a general callback framework without a concrete requirement.

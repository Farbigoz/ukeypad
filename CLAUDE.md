# CLAUDE.md

This file provides guidance for working with the `ukeypad` firmware project.

## Project scope

This is a PlatformIO Arduino firmware project for a general-purpose USB HID
keypad on an ESP32-S3 SuperMini. The current reference profile has six digital
mechanical switches:

- GPIO1 → Z
- GPIO2 → X
- GPIO3 → C
- GPIO4 → V
- GPIO5 → S
- GPIO6 → D

The hardware profiles are defined in [src/DeviceProfile.h](src/DeviceProfile.h).
The default reference profile has six inputs; a selectable eight-input
compile-time validation profile is also defined through `platformio.ini`.
Do not duplicate model, GPIO, default binding, scan, debounce, LED, or
capability values in runtime code or documentation. Future work is described
in [plan.md](plan.md): Hall inputs, RGB output, release hardening, and validated
MCU backends.

## Common commands

Run commands from the repository root:

```bash
pio run
pio run -t clean
pio run -t upload
pio run -t upload --upload-port COMx
pio device monitor

python -c "import re; s=open('utils/configurator.html',encoding='utf-8').read(); m=re.search(r'<script>(.*?)</script>',s,re.S); open('.configurator-check.js','w',encoding='utf-8').write(m.group(1))"
node --check .configurator-check.js
rm .configurator-check.js
python tools/build_configurator.py
python tools/build_configurator.py --check
python -c "from html.parser import HTMLParser; HTMLParser().feed(open('utils/configurator.html',encoding='utf-8').read()); print('HTML parsed')"

git status --short --branch
```

There is no automated unit-test or lint suite. Firmware validation is the
PlatformIO build plus hardware checks in [README.md](README.md). Never claim
hardware behavior was verified unless the device was actually tested.

For flashing, if native USB is not enumerated as an upload port, hold BOOT, tap
RESET, release BOOT, then run the upload command. The application uses USB-OTG/
TinyUSB and composite CDC+HID (`ARDUINO_USB_MODE=0`,
`ARDUINO_USB_CDC_ON_BOOT=1`).

## Repository layout

- `src/` — firmware.
- `src/DeviceProfile.h` — compile-time hardware/profile source of truth.
- `src/FirmwareVersion.h` — firmware, CDC protocol, and NVS format versions.
- `src/ConfigStorage.*` — versioned NVS binding records and CRC validation.
- `src/DeviceMetadata.*` — machine-readable CDC output.
- `src/KeyNameTable.*` — HID name lookup.
- `index.html` — GitHub Pages landing page.
- `utils/configurator.html` — generated standalone Web Serial GUI; do not edit directly.
- `utils/src/configurator/` — editable configurator template, CSS, and JavaScript sources (including `js/` fragments).
- `tools/build_configurator.py` — offline standard-library configurator bundler.
- `platformio.ini` — PlatformIO environment.
- `README.md` — build, wiring, protocol, and verification notes.
- `plan.md` — roadmap and design decisions.

GitHub Pages should use `master` and the repository root; keep links relative to the repository root.

## Firmware architecture

The latency-critical path is:

```text
Profile-defined timer ISR
  → Keypad::scan()
  → Button::update() for every configured input
  → integrator debounce
  → lock-free SPSC event queue
  → binary semaphore
  → main loop
  → HidKeyboard
  → USBHIDKeyboard pressRaw/releaseRaw
```

`Button` owns one physical input and debounce. It must not know about USB.
`Keypad` owns profile-defined inputs, bindings, scan, queue, runtime debounce,
and diagnostics; it must not know HID details. `HidKeyboard` owns USB reports
and duplicate-binding reference counts. `ConfigStorage` owns NVS serialization
but must not print CDC responses. `DeviceMetadata` owns protocol formatting but
must not own device configuration.

The event queue is single-producer/single-consumer: the timer ISR writes
`_head`, while the main task reads `_tail`. Never put blocking calls, Serial
output, NVS writes, or USB calls into the scan ISR.

`HidKeyboard` registers an `ARDUINO_USB_STOPPED_EVENT` callback. On USB
stop/disconnect it releases all HID state and clears duplicate-binding counts.

## Timer and ISR compatibility

The installed environment uses the legacy Arduino timer API:

```cpp
timerBegin(timerNumber, divider, countUp);
timerAttachInterrupt(timer, callback, edge);
timerAlarmWrite(timer, alarmTicks, autoreload);
timerAlarmEnable(timer);
```

Timer number, divider, target scan frequency, and derived alarm ticks are in
`DeviceProfile.h`. The current profile is 2000 Hz: APB 80 MHz / divider 80 gives
a 1 MHz timer tick and a 500-tick alarm. Do not replace the legacy API with the
newer timer API without checking the installed framework headers and rebuilding.

`IRAM_ATTR` is applied only to function definitions in `.cpp` files. GPIO reads
use `digitalRead()` for compatibility with the installed core; preserve this
unless a measured performance issue justifies a platform-specific backend.

## Unified operating mode

The firmware uses one operating mode. The profile-defined timer, GPIO scan,
integrator debounce, event queue, HID output, and CDC command channel are
active together:

- button events are forwarded to `HidKeyboard`;
- CDC commands are always available;
- `test on` additionally prints debounced events over CDC without suppressing
  HID output.

The CDC protocol is line-oriented and case-insensitive. Current commands:

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
boot
```

`boot` (also accepted as `download`) switches the ESP32-S3 into ROM download
mode through the installed TinyUSB restart API. It is issued from the main task,
not the scan ISR, and the physical BOOT + RESET procedure remains the fallback.

Responses must remain deterministic and machine-parseable: success begins with
`OK`, command errors use `ERR code=...`, and `list` ends with `OK list`.

Bindings are stored in NVS namespace `ukeypad`, key `bindings`, using the
versioned record implemented by `ConfigStorage`. The old raw six-byte format is
not supported or migrated. Debounce changes are persisted in the versioned NVS configuration record.

## Web configurator

`utils/configurator.html` uses Web Serial and requires a Chromium desktop browser
(Chrome/Edge). It reads `get_device` before rendering profile-sized binding rows.
When protocol responses change, update the GUI parser and README together. The
GUI must not duplicate profile GPIOs, input counts, or input types.

Edit the source fragments under `utils/src/configurator/`, then regenerate the
committed standalone artifact with `python tools/build_configurator.py`. Run
`python tools/build_configurator.py --check` before committing. The generated
HTML must remain dependency-free and usable directly through `file://`; do not
hand-edit it.

## Platform expansion rules

The current implementation is ESP32-S3-specific. Future backends must isolate
USB, timers, GPIO, ADC/DMA, and storage from the common input/event layers.
Do not promise support for a platform merely because an Arduino core exists.
A supported backend needs mature native USB HID+CDC, reproducible build/upload,
measured timer/input behavior, suitable ADC/DMA for Hall profiles, and reliable
nonvolatile storage.

Prefer a small common C API or static composition over virtual dispatch in the
latency path. Avoid general callback frameworks until a concrete requirement
exists.

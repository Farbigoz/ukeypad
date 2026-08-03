# ESP32-S3 USB HID Keypad

A general-purpose six-button USB HID keypad for games, shortcuts, macros,
media control, accessibility tools, testing, and other custom input workflows.
The original rhythm-game use case is only one possible application.

See [plan.md](plan.md) for the multi-hardware roadmap: compile-time device
profiles, Hall-effect inputs, mixed digital/Hall devices, capability-driven GUI,
and future RGB support.

A low-latency USB HID keyboard controller for 6 mechanical MX switches on an
**ESP32-S3 SuperMini**. Built with PlatformIO + Arduino framework, using the
ESP32 core's built-in **TinyUSB** (`USBHIDKeyboard`).

## Layout

| GPIO  | Key | HID usage |
|-------|-----|-----------|
| GPIO4  | Z   | 0x1D |
| GPIO5  | X   | 0x1B |
| GPIO6  | C   | 0x06 |
| GPIO7  | V   | 0x19 |
| GPIO15 | F13 | 0x68 |
| GPIO16 | F14 | 0x69 |

Each switch is wired between its GPIO and **GND**; internal pull-ups are used
(`INPUT_PULLUP`). Pressed == pin reads LOW. No external components required.

## Architecture

```
hardware timer ISR (2000 Hz)
        |
        v
   Keypad::scan()            direct GPIO register read + integrator debounce
        |                    -> pushes KeyEvents into a lock-free SPSC ring
        v                    -> gives a binary semaphore
   main loop (task)          blocks on the semaphore (no busy-wait)
        |                    drains the queue, forwards events to HID
        v
   HidKeyboard              USBHIDKeyboard.pressRaw/releaseRaw
                             -> immediate USB HID report (6-key rollover)
```

- **No `delay()`**, no slow `loop()` polling.
- **2000 Hz** GPIO scan via a hardware timer (500 µs alarm).
- **1 ms** USB HID polling interval (standard HID keyboard descriptor).
- **Integrator debounce** (no fixed-delay debounce): `INTEGRATOR_MAX = 4`
  samples at 2000 Hz → ~2 ms stability window. Tune in [Button.h](src/Button.h).
- **6-key rollover** out of the box (USBHIDKeyboard's internal report holds
  up to 6 simultaneous keys — exactly our key count).

### Layering

| Layer        | Knows about            | Does NOT know about |
|--------------|------------------------|---------------------|
| `Button`     | one GPIO + debounce   | USB, HID, other keys |
| `Keypad`     | 6 Buttons, scan, queue | USB, HID            |
| `HidKeyboard`| USB HID reports        | GPIOs, debounce     |

## Browser configurator

The repository includes [web/configurator.html](web/configurator.html), a standalone
GUI for the CDC configuration channel. It uses the browser's **Web Serial
API** — no server, npm, Python, or external libraries are required.

Supported browsers are current desktop **Google Chrome** and **Microsoft Edge**
(Web Serial is not generally available in Firefox or Safari). The page must be
opened in a secure context: `file://` usually works in Chrome/Edge, otherwise
serve the project directory locally, for example:

```bash
python -m http.server 8000
```

Then open `http://localhost:8000/web/` for the project page, or
`http://localhost:8000/web/configurator.html` for the configurator.

### Usage

1. Unplug the keypad.
2. Hold **any one button** while connecting USB to enter config mode.
3. Open `web/configurator.html` in Chrome/Edge.
4. Click **Подключить CDC** and select the keypad's CDC COM port.
5. Change key selectors; each change sends a `bind <slot> <key>` command.
6. Click **Сохранить в NVS** to persist the new bindings.
7. Use **test**, **debounce**, **stats**, and **info** controls as needed.
8. Disconnect USB or tap RESET without holding a key to return to normal mode.

The configurator mirrors the firmware's text protocol, including explicit
`OK ...` and `ERR code=...` responses. In config mode the firmware still runs
the 2000 Hz scan and integrator debounce; test events are reported by CDC and
never sent as HID keypresses. Duplicate bindings are allowed.

### GUI features

- read and edit all six GPIO slots;
- duplicate key assignments;
- `info` display;
- `test on/off` debounced button event log;
- read/set debounce samples;
- read/clear scan, event, overflow, and queue-depth statistics;
- reset defaults in RAM;
- save bindings to NVS;
- raw serial log for troubleshooting.

The browser cannot flash firmware and cannot access the device while it is in
normal keyboard mode if the firmware intentionally keeps CDC command handling
inert. Enter config mode first.

## Build

```bash
cd osu-keypad
pio run                 # build
pio run -t upload       # flash
pio device monitor      # serial monitor (UART0 @115200, see notes)
```

Requirements: PlatformIO with the `espressif32` platform. The hardware timer
API used is the core **2.x** style: `timerBegin(num, divider, countUp)`.

## Flashing the SuperMini

The SuperMini has **no separate USB-UART bridge** — its USB-C connector is the
ESP32-S3's native USB. To flash over that same cable:

1. Hold **BOOT**, tap **RESET**, release **BOOT** → the chip enters ROM
   download mode and the USB-Serial/JTAG port enumerates as a COM port.
2. `pio run -t upload` — esptool flashes over that port.
3. Tap **RESET** → the app boots; the USB port now presents the HID keyboard.

If PlatformIO doesn't auto-find the port, set it explicitly:
`pio run -t upload --upload-port COMx` (or `/dev/ttyACMx` on Linux).

> The first upload of a sketch that uses USB-OTG (TinyUSB) may look like it
> "hangs" if the previous firmware was in USB-Serial/JTAG mode. Use the
> BOOT+RESET combo above to force download mode.

## Choosing / verifying the board

The SuperMini is sold in several flash/PSRAM variants. This project assumes
the most common one: **4 MB QSPI flash, no PSRAM**. The config uses
`board = esp32-s3-devkitc-1` with overrides:

```ini
board_build.flash_size = 4MB
board_build.partitions  = default.csv
```

- **No PSRAM** by design — do **not** add `-DBOARD_HAS_PSRAM`.
- **8 MB flash** variant: set `board_build.flash_size = 8MB` and
  `board_build.partitions = default_8MB.csv`.
- If your SuperMini has PSRAM and you later want to use it, add
  `-DBOARD_HAS_PSRAM` and the matching `board_build.arduino.memory_type`
  (e.g. `qio_qspi` for QSPI PSRAM).

GPIOs 4/5/6/7/15/16 are safe — none are USB pins (19/20) or flash/PSRAM
SPI pins (26–32), and none are strapping pins (0/3/45/46).

## USB settings explained

These three are the usual source of confusion on ESP32-S3.

| Setting | Value here | Meaning |
|--------|-----------|---------|
| **USB Mode** (`ARDUINO_USB_MODE`) | `0` (USB-OTG / TinyUSB) | Routes the USB port through the **TinyUSB** stack. **Required for HID.** The board's default is `1` (USB-Serial/JTAG CDC only — CDC+JTAG, no HID), so it is **unflagged** in `build_unflags`. |
| **USB CDC On Boot** (`ARDUINO_USB_CDC_ON_BOOT`) | `1` | The device enumerates as a **composite** CDC + HID Keyboard. The CDC port is the **config channel** for bind commands (see "Configuring binds"). The HID endpoint is unaffected — reports still go out at ≤1 ms; adding CDC costs zero keystroke latency. |
| **Upload Mode** | ROM download via USB-Serial/JTAG | Not a build flag — it is the **physical BOOT+RESET** procedure described above. esptool flashes over the USB-Serial/JTAG COM port that the ROM bootloader exposes in download mode. |

Key point: **USB Mode = 0 (TinyUSB)** is what makes `USBHIDKeyboard` available
and functional. With the default USB Mode = 1, `USBHIDKeyboard` will not work.

## Configuring binds (config mode)

The device runs in one of two modes, selected at boot by whether a switch is
held. One firmware, two behaviours:

| Mode | How to enter | Scanning | CDC Serial |
|------|-------------|----------|------------|
| **Keyboard** (normal) | Plug in without holding any switch | ON (2000 Hz) | Inert |
| **Config** | Hold **any** switch while plugging in | ON (2000 Hz, debounce) | Accepts commands; HID output suppressed |

In config mode the timer, debounce and event queue are identical to normal
mode. Only the final HID forwarding step is disabled.

Saved bindings persist in **NVS** (flash) and survive power cycles in both
modes. To exit config mode, tap **RESET** without holding any key.

### Using the config channel

1. Unplug the keypad.
2. **Hold any switch down** and plug the USB cable in.
3. Open a serial terminal at **115200 baud** to the device's COM port
   (`pio device monitor`, PuTTY, Arduino Serial Monitor, etc.).
4. The banner prints once the terminal connects. Type commands:

```
bind 0 Z          # slot 0 sends Z
bind 4 F13        # slot 4 sends F13
bind 2 5          # slot 2 sends digit 5
list              # show current bindings
info              # show firmware/hardware/USB information
test on           # report raw GPIO press/release transitions
test off          # stop raw GPIO test

debounce get      # show debounce threshold in scan samples
debounce set 4    # set threshold (1..255 samples)
stats             # show scan/event/queue counters
stats clear       # reset diagnostic counters
save              # write bindings to NVS (persists across reboots)
reset             # restore compiled-in defaults (RAM only; 'save' to keep)
help              # show usage
```

Slots are **0..5**, matching the order in the binding table
(see `BINDINGS[]` in [Keypad.cpp](src/Keypad.cpp)). Key names are
case-insensitive (`f13` == `F13`).

**Supported key names:** A–Z, 0–9, F1–F24, ENTER, ESC, TAB, SPACE,
BACKSPACE, INSERT, DELETE, HOME, END, PAGEUP/PAGEDOWN, arrows
(UP/DOWN/LEFT/RIGHT), CAPSLOCK, PRINTSCREEN, SCROLLLOCK, MENU,
MUTE, VOLUP/VOLDN, CTRL/SHIFT/ALT/GUI/WIN, and the full HidKeycode
enum (see [HidKeycode.h](src/HidKeycode.h)).

5. When done, tap **RESET** without holding any key → keyboard mode.

### Hardware notes

- Config mode gates **HID output**, not scanning: the same 2000 Hz timer,
  GPIO sampling, integrator debounce and event queue run in both modes. In
  config mode events are consumed without reaching the host; `test on` prints
  the debounced events over CDC instead.
- The COM port appears whenever the device is plugged in (the composite
  descriptor is compile-time); in normal mode it is simply inert — no
  commands are read, no data is sent.
- Bindings only change what HID usage code a slot sends; the **physical
  GPIO → slot mapping is fixed** by the PCB and is not reassignable over
  Serial.

## Files

```
osu-keypad/
├── platformio.ini
└── src/
    ├── main.cpp          # boot-mode detection, timer, event loop
    ├── Button.h / .cpp   # one switch + integrator debounce
    ├── Keypad.h / .cpp   # scan + lock-free event queue + mutable binds
    ├── HidKeyboard.h/.cpp# USBHIDKeyboard adapter
    ├── HidKeycode.h      # named USB HID usage codes (enum class)
    └── Config.h / .cpp   # NVS persistence + CDC Serial command parser
```

## Checking / measuring latency

There is no single "latency" number — it is a chain. Verify each link:

1. **Scan rate (2000 Hz).** Toggle a spare GPIO inside the ISR and scope it,
   or count ISR firings per second and print over UART:
   ```cpp
   // in onScanTimer: g_isrCount++;
   // every 1000 ms (from a separate low-rate timer / loop tick):
   //   Serial.printf("scan Hz=%u\n", g_isrCount); g_isrCount = 0;
   ```
   Expect ~2000. Jitter should be a few µs (hardware timer).

2. **Debounce contribution.** `INTEGRATOR_MAX = 4` at 2000 Hz adds **~2 ms**
   of required stability before a Press is emitted. This is a deliberate
   trade-off vs. bounce noise. Lower it to shave latency on clean switches;
   raise it for noisy ones. It is the dominant latency term in this firmware.

3. **Firmware → USB event.** Scope a GPIO that you toggle in `handleEvent()`
   the instant a Press is forwarded; that's when the HID report leaves the
   chip. Compare to the physical contact edge.

4. **USB poll interval (1 ms).** The HID keyboard descriptor uses the default
   `bInterval = 1` ms (full-speed). On the host, use a USB analyzer
   (Wireshark + USBPcap on Windows, or `usbmon` on Linux) and inspect the IN
   endpoint interval. `pressRaw()` sends a report immediately, so the host
   receives it on the next 1 ms poll at the latest.

5. **End-to-end (gold standard).** Rig a known-good fast sensor (e.g. an
   opto-interrupter or a second MCU firing a GPIO) to close the same circuit
   and timestamp: contact edge → HID report received by the host.
   A simple host-side test app (raw `ReadFile` on the HID device, or
   `evdev`/`GetRawInputData`) logs the arrival time. Subtract the contact
   timestamp.

6. **NKRO / simultaneous keys.** Hold all 6 switches; a host HID test
   (e.g. https://configchecker.com or a custom raw-HID reader) should report
   all 6 keycodes present in the same report (USBHIDKeyboard supports up to
   6 simultaneous non-modifier keys, which is exactly our key count).

## Future development

- **RGB backlight (SK6812/WS2812).** Drive a strand via the RMT peripheral
  (`Adafruit_NeoPixel` or `FastLED` with an RMT-backed driver). Keep the LED
  refresh off the scan ISR — run it from a separate low-priority FreeRTOS
  task or a second timer. Avoid `delay()`/bit-banging; RMT is DMA-based and
  won't steal ISR cycles.
- **~~Configurable binds.~~** ✅ Done — `Config.h/.cpp` provides a text
  protocol over USB-CDC Serial with NVS persistence. Hold any switch at boot
  to enter config mode. See "Configuring binds" above.
- **RGB backlight (future task).** Planned for a later hardware/software
  revision. Use the ESP32-S3 RMT peripheral for WS2812/SK6812; never refresh
  LEDs inside the 2000 Hz scan ISR.
- **Macros / layers.** Replace the 1:1 event→key mapping in `HidKeyboard`
  with a small state machine: a key event can emit a timed sequence of HID
  usages. Drive timing from the same 2000 Hz timer (a macro scheduler tick)
  rather than `delay()`.
- **Hall-effect + Rapid Trigger (next hardware rev).** Replace the digital
  MX switches with analog Hall sensors (e.g. SS49E / TMAG5273) on ADC/I2C.
  The `Button` abstraction becomes a "HallKey" that reports a continuous
  position; debounce is replaced by hysteresis + a configurable **actuation
  depth** and **rapid-trigger release threshold** (re-trigger on any upward
  movement). The rest of the stack (Keypad queue, HID adapter) stays the
  same — only the sensor/decision layer changes. ADC sampling at 2000 Hz is
  feasible on the S3; budget the I2C variant carefully (may need a lower
  rate per sensor with round-robin, or an I2C multi-sensor bus).

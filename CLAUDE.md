# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Classic arcade emulator for the **DFRobot UNIHIKER K10** (ESP32-S3), built with PlatformIO on top of ESP-IDF v5.4. ROM data is compiled directly into firmware as C headers. Currently ships with Galaga enabled by default.

## Build Commands

```bash
pio run -e unihiker_k10_arcade          # Build
pio run -e unihiker_k10_arcade -t upload # Flash to device
pio device monitor -b 115200            # Serial monitor
pio run -e unihiker_k10_arcade -t fullclean  # Clean build artifacts
```

Alternative with ESP-IDF CLI: `idf.py build` / `idf.py flash` / `idf.py monitor`

The project uses `--recursive` submodules; the `components/` folder contains external dependencies (e.g., arduino-esp32). If cloning fresh, use `git clone --recursive`.

## Architecture

The codebase is layered:

1. **Entry point** (`main.cpp`) — `app_main()` creates a single FreeRTOS task that polls input and drives the state machine.
2. **State machine** (`k10_state.*`) — Owns menu navigation and game selection. `K10Machine` enum lists all supported games (MENU, PACMAN, GALAGA, DKONG, FROGGER, DIGDUG, 1942).
3. **Emulator bridge** (`main/emulator/k10_emulator.*`) — Sits between the app and the emulation core; manages per-frame execution and game lifecycle.
4. **Emulation core** (`main/arcade_core/emulation.c` + `main/arcade_core/config.h`) — Z80 CPU emulator and game-specific logic. Games are conditionally compiled via `#define ENABLE_*` macros.
5. **Hardware drivers** — `main/hardware/k10_hardware.*` (I2C/I2S/XL9535 expander), `main/hardware/k10_video.*` (ILI9341 TFT via SPI, double-buffering), `main/state/k10_wifi_gamepad.cpp` (NimBLE BLE HID host for 8BitDo gamepads).

### Key Configuration Files

| File | Purpose |
|------|---------|
| `main/config/k10_config.h` | All pin definitions, I2C/SPI/I2S config, display geometry, audio sample rates |
| `main/arcade_core/config.h` | `ENABLE_*` macros to toggle games at compile time |
| `main/state/k10_state.cpp` | Boot machine selection and menu flow |
| `platformio.ini` | PlatformIO build environment for the `unihiker_k10_arcade` target |
| `boards/unihiker_k10_arcade.json` | Custom PlatformIO board definition |
| `sdkconfig.unihiker_k10_arcade` | ESP-IDF Kconfig (generated; do not edit manually) |

### Hardware Peripherals (K10 Board)

- **I2C** (SDA=GPIO47, SCL=GPIO48): XL9535 IO expander @ `0x20` — controls backlight, reads onboard Key A/B buttons
- **SPI** (MOSI=21, SCLK=12, CS=14, DC=13): ILI9341 TFT display, 240×320, active area 224×288
- **I2S** (BCK=0, WS=38, DOUT=45, MCLK=3): Audio output
- **GPIO0**: Physical BOOT button (Key K1)

### ROM Data

ROMs are embedded as C header arrays in `main/arcade_core/`. The `romconv/` directory contains tools to convert raw ROM binary files into these headers. Add new game ROMs by converting them and placing the headers in `arcade_core/`.

### Adding a New Game

1. Convert ROM binaries using `romconv/` tools.
2. Place resulting `.h` files in `main/arcade_core/`.
3. Add an `#define ENABLE_<GAME>` guard in `arcade_core/config.h` and implement the machine logic in `emulation.c`.
4. Add a `K10Machine` enum entry in `main/state/k10_state.h` and wire it into `main/state/k10_state.cpp` and `main/emulator/k10_emulator.cpp`.

### Input

- Onboard buttons currently disabled (`K10_DISABLE_ONBOARD_BUTTONS=1`); input comes from BLE HID gamepad (8BitDo).
- Virtual button bitmasks are defined in `main/hardware/k10_input.h` (UP/DOWN/LEFT/RIGHT/FIRE/START/COIN/EXTRA).

### Audio Sample Rates

Different games run at different rates configured per-machine in `k10_config.h`:
- Galaga: 24 kHz
- Donkey Kong: ~11.765 kHz

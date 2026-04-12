# Unihiker K10 Arcade

Arcade emulator project for the [DFRobot](https://www.dfrobot.com/) [UNIHIKER K10](https://www.dfrobot.com/product-2823.html), built with [PlatformIO](https://platformio.org/) on top of [Espressif](https://www.espressif.com/)'s [ESP-IDF](https://github.com/espressif/esp-idf) framework.

Author: Terence Ang

Code reference: Galadino

## Overview

This project targets the UNIHIKER K10 and runs a classic arcade-style emulator on the board's ESP32-S3 hardware.

The codebase separates board support, video, audio, input handling, and the emulation core so it can be extended later for additional arcade machines.

## Current Status

- Board target: UNIHIKER K10
- Framework: ESP-IDF via PlatformIO
- Display: ILI9341 TFT, 240x320
- Audio output: I2S
- IO expander: XL9535
- Enabled machine builds: Pac-Man, Galaga, Donkey Kong, Frogger, Dig Dug, 1942
- Default menu preference: Galaga (used when no previous NVS selection exists)
- Menu state restores the last selected machine from NVS and supports idle auto-launch

## Features

- Six arcade machines in one firmware: Pac-Man, Galaga, Donkey Kong, Frogger, Dig Dug, and 1942
- Multi-machine emulator build controlled by compile-time `ENABLE_*` flags
- Zero-copy DMA double-buffered frame presentation to the TFT display
- Strip-based rendering (48-pixel strips) within ESP32-S3 SPI hardware limits
- I2S audio playback for arcade sound generation (Namco WSG, AY-3-8910, DK PCM)
- XL9535-based onboard button and backlight handling
- WiFi AP gamepad server for phone-based control
- App-level menu flow with NVS-persisted last selection and idle random-launch
- FreeRTOS dual-core: emulation on core 0, rendering/present on core 1

## Hardware Notes

The project is configured for the UNIHIKER K10 board hardware:

- I2C expander on address `0x20`
- TFT active area tuned for the K10 display layout
- I2S audio pins configured for the K10 audio path
- Onboard Key A and Key B read through the XL9535 expander

Pin and hardware constants are centralized in `main/config/k10_config.h`.

## Controls

Input is abstracted through `main/hardware/k10_input.h` virtual button bits (`K10_BUTTON_*`).

In-game control behavior implemented in the state layer:

- `START + COIN`: Return to menu
- `FIRE` or `START` or `COIN` from menu: Launch selected game

Onboard button handling can be disabled by configuration:

- `K10_DISABLE_ONBOARD_BUTTONS` in `main/config/k10_config.h` is currently set to `1`.

## Build Requirements

- Visual Studio Code with PlatformIO, or a working PlatformIO CLI installation
- Python environment compatible with PlatformIO
- ESP32 toolchain installed by PlatformIO

This project uses the custom board definition in `boards/unihiker_k10_arcade.json` and the PlatformIO configuration in `platformio.ini`.

## Build and Flash

Build the firmware:

```powershell
pio run -e unihiker_k10_arcade
```

Upload the firmware:

```powershell
pio run -e unihiker_k10_arcade -t upload
```

Open the serial monitor:

```powershell
pio device monitor -b 115200
```

Clean the build:

```powershell
pio run -e unihiker_k10_arcade -t fullclean
```

## Project Structure

```text
.
|- boards/
|  `- unihiker_k10_arcade.json
|- docs/
|- main/
|  |- arcade_core/        # Emulator core, ROM data, tables, game-specific logic
|  |  |- cpu/             # CPU emulators and opcode tables
|  |  |  |- z80/
|  |  |  `- i8048/
|  |  |- games/           # Per-game assets and machine implementations
|  |  |  |- pacman/
|  |  |  |- galaga/
|  |  |  |- dkong/
|  |  |  |- frogger/
|  |  |  |- digdug/
|  |  |  `- _1942/
|  |  `- emulation/       # Shared emulator orchestration modules
|  |- emulator/           # Runtime bridge between app and emulation core
|  |- hardware/           # K10 hardware setup, expander access, audio, inputs
|  |- state/              # Menu state, boot mode, and game flow
|  |- config/             # Board and runtime configuration headers
|  `- main.cpp            # ESP-IDF entry point and main FreeRTOS application task
|- platformio.ini
`- CMakeLists.txt
```

### Emulator Core Layout

`main/arcade_core/emulation.c` is intentionally thin and delegates into focused modules under `main/arcade_core/emulation/`.

Game-specific machine files and assets are now grouped by title under `main/arcade_core/games/`.

Shared emulation modules:

- `emulation_state.inc`: shared emulator state and globals
- `emulation_cpu_hooks.inc`: Z80 callback hook wiring and memory/IO dispatch integration
- `emulation_dispatch.inc`: `RdZ80`/`WrZ80`/`InZ80`/`OutZ80`
- `emulation_digdug_dkong.inc`: Dig Dug Namco I/O and DKong 8048 audio bridge helpers
- `emulation_menu.inc`: menu-frame logic
- `emulation_run_games.inc`: per-machine frame execution dispatch
- `emulation_orchestration.inc`: menu-vs-game routing
- `emulation_lifecycle.inc`: reset/startup lifecycle (`emulation_reset`, `prepare_emulation`)
- `emulation_frame_sync.inc`: frame pacing and attract timeout checks

## Configuration

Important configuration points:

- `main/arcade_core/config.h`: enabled arcade machines and low-level emulator options
- `main/state/k10_state.cpp`: boot machine, menu state, NVS selection restore, and game flow
- `main/config/k10_config.h`: K10 pin map, display geometry, audio rates, and expander layout

At the moment:

- `ENABLE_PACMAN`, `ENABLE_GALAGA`, `ENABLE_DKONG`, `ENABLE_FROGGER`, `ENABLE_DIGDUG`, and `ENABLE_1942` are active
- Default machine fallback is `K10_MACHINE_GALAGA`
- Menu idle-launch timeout is enabled (`MASTER_ATTRACT_MENU_TIMEOUT` in `main/arcade_core/config.h`)

## Performance Tuning

The `sdkconfig.unihiker_k10_arcade` is tuned for speed over size:

| Setting | Value |
|---------|-------|
| CPU frequency | 240 MHz |
| Compiler optimization | `-O2` (performance) |
| Flash mode | QIO (2× bandwidth vs DIO) |
| Instruction cache | 32 KB |
| Data cache | 64 KB / 64-byte lines |
| SPI master driver | Placed in IRAM |
| Assertions | Disabled |

Hot rendering and audio functions are marked `IRAM_ATTR` for zero-wait execution from internal RAM.

## Notes for Development

- Additional games require emulator support plus ROM/header integration under `main/arcade_core/games/<game>/`
- New games must add a `current_rom_base` case in `k10_emulator_start()` — `prepare_emulation()` uses a compile-time `#if` chain that only picks the first enabled game; the runtime switch overrides it
- Strip height × width × 2 must not exceed 32,767 bytes (ESP32-S3 SPI hardware max per transaction)
- Zero-copy DMA: `k10_video_write()` transmits directly from the caller's buffer (must be DMA-capable)
- Hardware bring-up, audio timing, and display timing are tuned specifically for the K10 target

## Credits & Attribution

- **Author:** Terence Ang
- **Based on:** [Galagino](https://github.com/harbaum/galagino) by Till Harbaum — a multi-arcade emulator for ESP32
- **Platform:** [ESP-IDF](https://github.com/espressif/esp-idf) v5.4 by [Espressif](https://www.espressif.com/) and [PlatformIO](https://platformio.org/)
- **Emulation core:** Z80 CPU emulator and game-specific logic derived from Galagino
- **Original arcade games:** Pac-Man (Namco), Galaga (Namco), Donkey Kong (Nintendo), Frogger (Konami/Sega), Dig Dug (Namco), 1942 (Capcom). All trademarks are property of their respective owners. No ROM files are included in this repository.

## License

This project is licensed under the **GNU General Public License v3.0 (GPLv3)**, the same license as the original [Galagino](https://github.com/harbaum/galagino) project by Till Harbaum.

See [LICENSE](LICENSE) for the full license text.

### Summary

- You are free to use, modify, and distribute this software.
- Any derivative work must also be released under GPLv3.
- The source code must be made available when distributing the software.
- No warranty is provided.

ROM images are **not** included and are the property of their respective copyright holders.

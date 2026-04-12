# Unihiker K10 Arcade

Arcade emulator project for the DFRobot UNIHIKER K10, built with PlatformIO on top of the ESP-IDF framework.

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

- Multi-machine emulator build controlled by compile-time `ENABLE_*` flags
- Double-buffered frame presentation to the TFT display
- I2S audio playback for arcade sound generation
- XL9535-based onboard button and backlight handling
- App-level menu flow with persisted last selection and idle random-launch

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

## Notes for Development

- Additional games require emulator support plus ROM/header integration under `main/arcade_core/`
- Hardware bring-up, audio timing, and display timing are tuned specifically for the K10 target

## Credits

- Author: Terence Ang
- Code reference: Galadino
- Platform foundation: ESP-IDF and PlatformIO

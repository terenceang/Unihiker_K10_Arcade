# Unihiker K10 Arcade

Arcade emulator project for the DFRobot UNIHIKER K10, built with PlatformIO on top of the ESP-IDF framework.

Author: Terence Ang

Code reference: Galadino

## Overview

This project targets the UNIHIKER K10 and runs a classic arcade-style emulator on the board's ESP32-S3 hardware. The current configuration is focused on Galaga and boots directly into that game.

The codebase separates board support, video, audio, input handling, and the emulation core so it can be extended later for additional arcade machines.

## Current Status

- Board target: UNIHIKER K10
- Framework: ESP-IDF via PlatformIO
- Display: ILI9341 TFT, 240x320
- Audio output: I2S
- IO expander: XL9535
- Default game: Galaga
- Menu and additional machine entries exist in the app layer, but the current emulator runtime only supports Galaga

## Features

- Galaga emulation integrated directly into the firmware image
- Double-buffered frame presentation to the TFT display
- I2S audio playback for arcade sound generation
- XL9535-based onboard button and backlight handling
- Lightweight application layer that can switch between menu mode and emulator mode

## Hardware Notes

The project is configured for the UNIHIKER K10 board hardware:

- I2C expander on address `0x20`
- TFT active area tuned for the K10 display layout
- I2S audio pins configured for the K10 audio path
- Onboard Key A and Key B read through the XL9535 expander

Pin and hardware constants are centralized in `main/k10_config.h`.

## Controls

In the current build, the onboard buttons are mapped as follows:

- Key A: Fire
- Key B: Coin
- Key A + Key B: Start
- Key A + Key B during gameplay: Restart or return action, depending on app mode

Direction button enums and menu navigation paths already exist in the code, but the present hardware input layer is primarily set up for the onboard A/B buttons.

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
|  |- k10_state.*         # Menu state, boot mode, and game flow
|  |- k10_emulator.*      # Runtime bridge between app and emulation core
|  |- k10_hardware.*      # K10 hardware setup, expander access, audio, inputs
|  |- k10_video.*         # Display startup and frame output
|  `- main.cpp            # ESP-IDF entry point and main FreeRTOS application task
|- platformio.ini
`- CMakeLists.txt
```

## Configuration

Important configuration points:

- `main/arcade_core/config.h`: enabled arcade machines and low-level emulator options
- `main/k10_state.cpp`: boot machine, menu state, and game flow
- `main/k10_config.h`: K10 pin map, display geometry, audio rates, and expander layout

At the moment:

- `ENABLE_GALAGA` is active
- The boot machine is set to `K10_MACHINE_GALAGA`
- Only Galaga is reported as supported by the runtime

## Notes for Development

- The app layer already includes placeholders for Pac-Man, Donkey Kong, Frogger, Dig Dug, and 1942
- Additional games will require emulator support and any needed ROM/header integration
- Hardware bring-up, audio timing, and display timing are tuned specifically for the K10 target

## Credits

- Author: Terence Ang
- Code reference: Galadino
- Platform foundation: ESP-IDF and PlatformIO

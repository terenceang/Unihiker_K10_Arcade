# GEMINI.md - Unihiker K10 Arcade Project Context

## Project Overview
**Unihiker K10 Arcade** is a specialized arcade emulator designed for the **Unihiker K10** (an ESP32-S3 based development board). It is built upon a template that combines **ESP-IDF v5.4** and the **Arduino Core for ESP32**.

### Key Features
- **Emulation:** Supports six classic arcade machines: Pac-Man, Galaga, Donkey Kong, Frogger, Dig Dug, and 1942. All are enabled by default. Logic and ROMs are integrated via C headers in `main/arcade_core/games/`.
- **Hardware Integration:**
  - **Display:** ILI9341 TFT (240x320), managed via SPI with zero-copy DMA double-buffering.
  - **IO Expander:** XL9535 for backlight control and reading onboard buttons.
  - **Audio:** I2S audio output.
- **Architecture:** 
  - The project boots into an ESP-IDF environment.
  - `main.cpp` provides the ESP-IDF entry point and the main FreeRTOS application task.
  - `k10_state.cpp` owns menu state, boot mode selection, and high-level input transitions.
  - A menu system allows selecting between all enabled games, with NVS-persisted last selection and idle auto-launch.

## Building and Running

### Prerequisites
- **PlatformIO** (recommended) or **ESP-IDF v5.4.2**.
- **Recursive Clone:** The project relies on submodules in the `components` folder. Ensure you clone with `--recursive`.

### Commands

#### PlatformIO
- **Build:** `pio run -e unihiker_k10_arcade`
- **Upload:** `pio run -t upload -e unihiker_k10_arcade`
- **Monitor:** `pio device monitor`

#### ESP-IDF CLI
- **Build:** `idf.py build`
- **Flash:** `idf.py flash`
- **Monitor:** `idf.py monitor`

## Development Conventions

### Code Structure
- **`main/`**: Contains the core application and hardware abstraction layer.
  - `k10_hardware.cpp`: Low-level hardware initialization (I2C, SPI, I2S, XL9535).
  - `arcade_core/`: The emulation engine and ROM data.
    - `games/`: Per-game assets organized by title (pacman/, galaga/, dkong/, frogger/, digdug/, _1942/).
    - `emulation/`: Shared orchestration modules (.inc files).
    - `cpu/`: Z80 and i8048 CPU emulators.
  - `emulator/`: Runtime bridge between app and emulation core.
  - `hardware/`: K10 hardware drivers (video, audio, input).
  - `state/`: Menu state machine and WiFi gamepad server.
  - `config/`: Board and runtime configuration headers.
- **components/**: External dependencies like `arduino`.

### Performance Tuning
The sdkconfig is tuned for speed:
- CPU: 240 MHz, compiler: `-O2`
- Flash: QIO mode, instruction cache: 32 KB, data cache: 64 KB / 64-byte lines
- SPI master driver in IRAM, assertions disabled
- Hot render and audio functions marked `IRAM_ATTR`
- Zero-copy DMA rendering (no staging buffer)

### Adding/Modifying Games
- Emulation settings are found in `main/arcade_core/config.h`.
- Games are enabled/disabled via `#define` macros (e.g., `ENABLE_GALAGA`) in `main/arcade_core/config.h`.
- ROMs must be converted to C headers and placed in the `main/arcade_core/games/<game>/` directory.
- New games also need a `current_rom_base` case in `k10_emulator_start()` for correct Z80 instruction fetch.

### Hardware Mapping
- Pin definitions are centralized in `main/config/k10_config.h`.
- Backlight and onboard buttons are accessed via the XL9535 IO Expander at I2C address `0x20`.

### Troubleshooting

- **`current_rom_base` bug**: When multiple games are enabled, `prepare_emulation()` only sets `current_rom_base` for the first game in a compile-time `#if` chain. `k10_emulator_start()` overrides this at runtime. If a new game is added without a case there, it will execute the wrong ROM.
- **Strip height limit**: `K10_TFT_STRIP_HEIGHT × K10_TFT_ACTIVE_WIDTH × 2` must not exceed 32,767 bytes (ESP32-S3 SPI hardware max). Max valid strip height is 48.

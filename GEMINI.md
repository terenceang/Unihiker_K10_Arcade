# GEMINI.md - Unihiker K10 Arcade Project Context

## Project Overview
**Unihiker K10 Arcade** is a specialized arcade emulator designed for the **Unihiker K10** (an ESP32-S3 based development board). It is built upon a template that combines **ESP-IDF v5.4** and the **Arduino Core for ESP32**.

### Key Features
- **Emulation:** Supports multiple classic arcade machines, with **Galaga** currently enabled by default. Logic and ROMs are integrated via C headers in `main/arcade_core`.
- **Hardware Integration:**
  - **Display:** ILI9341 TFT (240x320), managed via SPI.
  - **IO Expander:** XL9535 for backlight control and reading onboard buttons.
  - **Audio:** I2S audio output.
- **Architecture:** 
  - The project boots into an ESP-IDF environment.
  - `main.cpp` provides the ESP-IDF entry point and the main FreeRTOS application task.
  - `k10_state.cpp` owns menu state, boot mode selection, and high-level input transitions.
  - It maintains a "Menu" state for selecting games (though currently focused on Galaga).

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
- **components/**: External dependencies like `arduino`.

### Adding/Modifying Games
- Emulation settings are found in `main/arcade_core/config.h`.
- Games are enabled/disabled via `#define` macros (e.g., `ENABLE_GALAGA`).
- ROMs must be converted to C headers and included in the `arcade_core` directory.

### Hardware Mapping
- Pin definitions are centralized in `main/config/k10_config.h`.
- Backlight and onboard buttons are accessed via the XL9535 IO Expander at I2C address `0x20`.

### Troubleshooting

- **Optimization:** `sdkconfig.defaults` disables some optimizations to work around known bugs in `arduino-esp32`.

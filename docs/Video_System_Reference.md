# Video System Reference

This document describes the video architecture of the Unihiker K10 Arcade emulator, covering the hardware interface, rendering pipeline, and performance optimizations.

## 1. Hardware Interface

The Unihiker K10 uses an **ILI9341 TFT display** connected via a high-speed SPI bus.

### Configuration
- **Resolution:** 240x320 pixels.
- **Active Area:** 224x288 pixels (centered for arcade games like Galaga).
- **Margins:** 8-pixel horizontal and 16-pixel vertical margins.
- **SPI Clock:** 80MHz.
- **Color Depth:** RGB565 (16-bit).

### Pin Mapping (`k10_config.h`)
- **MOSI:** GPIO 21
- **SCLK:** GPIO 12
- **CS:** GPIO 14
- **DC:** GPIO 13
- **Backlight:** Managed via the **XL9535 IO Expander** (Port 0, Bit 0).

## 2. Rendering Pipeline

The system uses a memory-efficient, strip-based rendering approach to accommodate the limited SRAM of the ESP32-S3 while maintaining high performance.

### Strip-Based Rendering
Instead of a full-frame buffer (which would require 240 * 320 * 2 = 153.6 KB), the system renders the screen in **8-pixel high strips** (224x8 pixels).
- **DMA Double Buffering:** Two DMA-capable buffers are used. While the SPI peripheral transmits one strip, the CPU renders the next.
- **Flow:**
  1. `k10_video_begin_frame()` sets the address window on the ILI9341.
  2. For each of the 36 strips (288 / 8):
     - CPU renders game logic into `buffer_A`.
     - CPU triggers DMA transfer for `buffer_A`.
     - CPU starts rendering next strip into `buffer_B`.
  3. `k10_video_end_frame()` closes the SPI transaction.

## 3. Emulation Integration

The video system is tightly coupled with the arcade core rendering logic.

### Layering Order
Within each 8-pixel strip, the emulator (`galaga.h`) draws layers in the following order to handle transparency and priority:
1. **Background (Stars):** Dynamic starfield pixels.
2. **Sprites:** 16x16 moving objects (ships, enemies).
3. **Tiles:** 8x8 character tiles for UI (scores, text).

### Memory Mapping
Arcade video RAM and sprite RAM are mapped into a flat `memory` buffer. The Z80 CPUs update this buffer during the "Emulation Task," and the "Presentation Task" reads from it to generate the RGB565 pixel data for the display.

## 4. Performance Optimizations

- **80MHz SPI:** Pushes the ILI9341 to its physical limits for minimal latency.
- **Skip Blanks:** The tile renderer skips "blank" tiles (specifically index 0x24 in Galaga) to save CPU cycles.
- **Task Separation:**
  - **Core 0:** Runs the `emulation_task` (Z80 execution).
  - **Core 1 (Main):** Runs the `presentation_task` (Rendering and SPI DMA management).
- **DMA Pipelining:** Ensures the SPI bus is almost 100% utilized during frame transmission.

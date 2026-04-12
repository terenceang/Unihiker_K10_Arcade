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
Instead of a full-frame buffer (which would require 240 * 320 * 2 = 153.6 KB), the system renders the screen in **48-pixel high strips** (224×48 pixels = 21,504 bytes per strip, 6 strips per frame).

The strip height of 48 is the maximum allowed by the ESP32-S3 SPI hardware limit of 262,143 bits (~32,767 bytes) per transaction. Each strip is subdivided into 3 pairs of 2 tile rows (16 pixels per pair), with game-specific `render_row()` functions drawing tiles and sprites.

- **Zero-Copy DMA Double Buffering:** Two DMA-capable frame buffers are allocated. While the SPI peripheral transmits one strip via DMA, the CPU renders the next into the other buffer. No staging memcpy is performed — the render buffer is transmitted directly.
- **Flow:**
  1. `k10_video_begin_frame()` sets the address window on the ILI9341 (224×288 active area offset by 8,16).
  2. For each of the 6 strips:
     - CPU gets the free draw buffer via `k10_video_get_draw_buffer()`.
     - `render_line()` clears the buffer and renders 3 pairs of tile rows.
     - `k10_video_write()` queues a DMA transfer directly from the render buffer.
  3. `k10_video_end_frame()` flushes pending DMA and releases CS.

- **Border clearing:** `k10_video_clear_border()` paints the border strips (outside the 224×288 active area) to black at each game start.

## 3. Emulation Integration

The video system is tightly coupled with the arcade core rendering logic.

### Layering Order
Within each strip, the per-game render function (e.g., `galaga_render_row`, `dkong_render_row`) draws layers in game-specific order. For example, Galaga draws:
1. **Background (Stars):** Dynamic starfield pixels.
2. **Sprites:** 16x16 moving objects (ships, enemies).
3. **Tiles:** 8x8 character tiles for UI (scores, text).

Donkey Kong uses a different layer order with black-pixel sprite masking.

### Memory Mapping
Arcade video RAM and sprite RAM are mapped into a flat `memory` buffer. The Z80 CPUs update this buffer during the "Emulation Task," and the "Presentation Task" reads from it to generate the RGB565 pixel data for the display.

## 4. Performance Optimizations

- **80MHz SPI:** Pushes the ILI9341 to its physical limits for minimal latency.
- **Zero-copy DMA:** No staging buffer — the render buffer is transmitted directly, saving ~21 KB of DMA RAM.
- **Skip Blanks:** Tile renderers skip known-blank tiles (e.g., index 0x24 in Galaga, 0x10 in Donkey Kong) to save CPU cycles.
- **IRAM_ATTR:** Hot rendering functions (`render_line`, `render_line_pair`, `buttons_get`, `snd_render_buffer_cpp`, `clamp_pcm16`) are placed in IRAM for zero-wait execution.
- **Task Separation:**
  - **Core 0:** Runs the `emulation_task` (Z80 execution, priority 3).
  - **Core 1 (Main):** Runs the `presentation_task` (Rendering and SPI DMA management).
- **DMA Pipelining:** At most one queued transfer in-flight, ensuring the SPI bus is almost 100% utilized during frame transmission.
- **sdkconfig tuning:** 240 MHz CPU, -O2, QIO flash, 32 KB icache, 64 KB dcache / 64-byte lines, SPI master in IRAM, assertions disabled.

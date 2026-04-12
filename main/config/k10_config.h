#ifndef K10_CONFIG_H
#define K10_CONFIG_H

#include <stdint.h>

// -----------------------------------------------------------------------------
// Board Buses and Core Peripherals
// -----------------------------------------------------------------------------

// I2C bus wiring for the XL9535 IO expander.
#define K10_I2C_SDA 47
#define K10_I2C_SCL 48
#define K10_EXPANDER_ADDR 0x20

// I2S audio output pins.
#define K10_I2S_BCK 0
#define K10_I2S_WS 38
#define K10_I2S_DOUT 45
#define K10_I2S_MCLK 3

// -----------------------------------------------------------------------------
// TFT Panel (ILI9341)
// -----------------------------------------------------------------------------

// SPI wiring.
#define K10_TFT_SPI_HOST SPI3_HOST
#define K10_TFT_SPICLK 80000000
#define K10_TFT_MISO -1
#define K10_TFT_MOSI 21
#define K10_TFT_SCLK 12
#define K10_TFT_CS 14
#define K10_TFT_DC 13
#define K10_TFT_RST -1
#define K10_TFT_ENABLE 40

// Panel orientation and viewport placement.
#define K10_TFT_MADCTL 0x48
#define K10_TFT_X_OFFSET 8
#define K10_TFT_Y_OFFSET 16

// Full panel dimensions.
#define K10_TFT_WIDTH 240
#define K10_TFT_HEIGHT 320

// Active game/menu area dimensions.
#define K10_TFT_ACTIVE_WIDTH 224
#define K10_TFT_ACTIVE_HEIGHT 288

// Strip-based DMA rendering geometry.
// Larger strips spend more DMA-capable RAM but reduce per-frame SPI/DMA overhead.
// Keep this a multiple of 16 and an exact divisor of K10_TFT_ACTIVE_HEIGHT.
// Max strip size is limited by ESP32-S3 SPI hardware: 262143 bits (~32 KB).
// 48 is the largest valid value (224*48*2 = 21504 bytes < 32767).
#define K10_TFT_STRIP_HEIGHT 48
#define K10_TFT_STRIP_COUNT (K10_TFT_ACTIVE_HEIGHT / K10_TFT_STRIP_HEIGHT)

// Global panel background color (RGB565, byte-swapped as used by the renderer).
#define K10_PANEL_BACKGROUND_COLOR 0x0000

// -----------------------------------------------------------------------------
// Menu Viewport / Border Masking
// -----------------------------------------------------------------------------

// Menu viewport rectangle (inside active area). Pixels outside this rectangle
// are forced to background color to hide embedded logo frame borders.
#define K10_MENU_VIEWPORT_MARGIN_X 12
#define K10_MENU_VIEWPORT_MARGIN_Y 12
#define K10_MENU_VIEWPORT_X K10_MENU_VIEWPORT_MARGIN_X
#define K10_MENU_VIEWPORT_Y K10_MENU_VIEWPORT_MARGIN_Y
#define K10_MENU_VIEWPORT_WIDTH (K10_TFT_ACTIVE_WIDTH - 2 * K10_MENU_VIEWPORT_MARGIN_X)
#define K10_MENU_VIEWPORT_HEIGHT (K10_TFT_ACTIVE_HEIGHT - 2 * K10_MENU_VIEWPORT_MARGIN_Y)
#define K10_MENU_VIEWPORT_MASK_COLOR K10_PANEL_BACKGROUND_COLOR

// -----------------------------------------------------------------------------
// Audio Defaults
// -----------------------------------------------------------------------------

// Per-machine audio rates.
#define K10_GALAGA_AUDIO_SAMPLE_RATE 24000
#define K10_DKONG_AUDIO_SAMPLE_RATE 11765

// System-wide defaults.
#define K10_AUDIO_VOLUME 30       // 0 (mute) to 100 (full)
#define K10_AUDIO_BUFFER_FRAMES 128 // Samples per synthesis/DMA chunk; larger chunks reduce submit overhead
#define K10_AUDIO_BUFFER_BYTES (K10_AUDIO_BUFFER_FRAMES * 2 * sizeof(int16_t)) // Stereo 16-bit

// -----------------------------------------------------------------------------
// XL9535 IO Expander
// -----------------------------------------------------------------------------

// XL9535 register addresses.
#define XL9535_INPUT_REG_0 0x00
#define XL9535_INPUT_REG_1 0x01
#define XL9535_OUTPUT_REG_0 0x02
#define XL9535_OUTPUT_REG_1 0x03
#define XL9535_CONFIG_REG_0 0x06
#define XL9535_CONFIG_REG_1 0x07

// XL9535 bit assignments used by the board.
// The K10 has 3 physical buttons: Key A, Key B, and a BOOT button (GPIO0).
#define K10_BIT_BACKLIGHT 0
#define K10_BIT_CAMERA_HOLD 1
#define K10_BIT_KEY_B 2  // Physical Button K4 (Port 0)
#define K10_BIT_KEY_A 4  // Physical Button K3 (Port 1)
#define K10_BIT_USER_LED 7
#define K10_GPIO_BOOT 0  // Physical Button K1 (on ESP32-S3 directly)

// Pre-shifted masks for the expander pins above.
#define K10_MASK_BACKLIGHT (1U << K10_BIT_BACKLIGHT)
#define K10_MASK_CAMERA_HOLD (1U << K10_BIT_CAMERA_HOLD)
#define K10_MASK_KEY_B (1U << K10_BIT_KEY_B)
#define K10_MASK_KEY_A (1U << K10_BIT_KEY_A)
#define K10_MASK_USER_LED (1U << K10_BIT_USER_LED)

// Port defaults: backlight and camera hold are outputs on port 0, LED is output on port 1.
#define K10_EXPANDER_OUTPUT_PORT0 (K10_MASK_BACKLIGHT | K10_MASK_CAMERA_HOLD)
#define K10_EXPANDER_CONFIG_PORT0 ((uint8_t)~K10_EXPANDER_OUTPUT_PORT0)
#define K10_EXPANDER_OUTPUT_PORT1 0x00
#define K10_EXPANDER_CONFIG_PORT1 ((uint8_t)~K10_MASK_USER_LED)

// -----------------------------------------------------------------------------
// Input / UI Behavior
// -----------------------------------------------------------------------------

// Set to 1 to ignore the onboard button inputs entirely.
// Currently set to 1 as button functions are temporarily disabled by the user.
#define K10_DISABLE_ONBOARD_BUTTONS 1

#define K10_LED_BRIGHTNESS 50   // 0 to 100

#endif
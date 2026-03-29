#ifndef K10_INPUT_H
#define K10_INPUT_H

#include <stdint.h>

// Virtual button bitmask for application and emulation logic.
// On the K10 hardware, these are currently mapped as:
// - K10_BUTTON_FIRE:  Key A (K3)
// - K10_BUTTON_COIN:  Key B (K4)
// - K10_BUTTON_START: Key A + Key B combo
// - K10_BUTTON_EXTRA: BOOT button (K1) - (Not currently mapped in hardware layer)
// Directions (UP/DOWN/LEFT/RIGHT) have no physical mapping on the K10 board itself.
enum : uint8_t {
    K10_BUTTON_LEFT = 0x01,
    K10_BUTTON_RIGHT = 0x02,
    K10_BUTTON_UP = 0x04,
    K10_BUTTON_DOWN = 0x08,
    K10_BUTTON_FIRE = 0x10,
    K10_BUTTON_START = 0x20,
    K10_BUTTON_COIN = 0x40,
    K10_BUTTON_EXTRA = 0x80,
};

#ifdef __cplusplus
extern "C" {
#endif

// Gamepad state struct for WiFi/BLE/other input sources
typedef struct {
    uint16_t buttons; // Bitmask of buttons (A, B, Start, Coin, etc)
    uint8_t hat;      // D-pad/hat value (0-7 = directions, 8 = center)
    int8_t lx, ly;    // Left stick X/Y (-128..127)
    int8_t rx, ry;    // Right stick X/Y (-128..127)
    bool connected;   // True if controller is active (optional for WiFi)
} k10_gamepad_state_t;

#ifdef __cplusplus
}
#endif

#endif
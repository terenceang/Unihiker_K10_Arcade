#ifndef K10_BT_H
#define K10_BT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Decoded Gamepad State
typedef struct {
    uint16_t buttons; // Bitmask: A, B, X, Y, L, R, Start, Select, etc.
    int8_t lx;        // Left Stick X (-128 to 127)
    int8_t ly;        // Left Stick Y
    int8_t rx;        // Right Stick X
    int8_t ry;        // Right Stick Y
    uint8_t hat;      // D-Pad (0-7, 8=center)
    bool connected;
} k10_gamepad_state_t;

// Initialize BLE HID Host (Central)
bool k10_bt_begin();

// Get the latest gamepad state
void k10_bt_get_gamepad_state(k10_gamepad_state_t *state);

#ifdef __cplusplus
}
#endif

#endif // K10_BT_H

#pragma once
#include "k10_input.h"

// Start WiFi gamepad receiver (AP + HTTP server)
void k10_wifi_gamepad_begin(void);

// Get latest gamepad state (thread-safe)
void k10_wifi_gamepad_get_state(k10_gamepad_state_t *state);

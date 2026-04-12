# Unihiker K10 Arcade - Button Mapping & Input Guide

This document describes how inputs are mapped to the arcade emulator's virtual buttons.

## Input Sources

The emulator supports multiple input sources:

1. **WiFi Gamepad (Primary):** A phone-based gamepad served via WiFi AP + HTTP. The K10 creates an access point and serves a touch gamepad webpage. This provides full directional + button input.
2. **BLE HID Gamepad:** 8BitDo or compatible Bluetooth gamepads via BLE HID.
3. **Onboard Buttons (Disabled by default):** Physical buttons on the K10 board.

## Physical Hardware Layout

The Unihiker K10 (DFR0992) features three primary physical buttons:

1.  **Key A (K3)**: Connected to the XL9535 IO Expander (Port 1, Bit 4).
2.  **Key B (K4)**: Connected to the XL9535 IO Expander (Port 0, Bit 2).
3.  **BOOT (K1)**: Connected directly to ESP32-S3 GPIO 0.

## Virtual Button Mapping

Because classic arcade games typically require more inputs (Joystick 4-way, Start, Coin, Fire) than the K10 provides physically, the following mapping is used in the software:

| Physical Input | Virtual Button | Arcade Function (Default) |
| :--- | :--- | :--- |
| **Key A** | `K10_BUTTON_FIRE` | Fire / Action |
| **Key B** | `K10_BUTTON_COIN` | Insert Coin |
| **Key A + Key B** | `K10_BUTTON_START` | Start Game |
| **BOOT (K1)** | `K10_BUTTON_EXTRA` | Currently Unmapped / Reserved |

### Directional Inputs
The Unihiker K10 does **not** have a physical D-pad or Joystick.
- Directional input comes from the WiFi gamepad or a BLE HID gamepad.
- Onboard buttons alone cannot provide movement.
- The virtual button bitmasks for `UP`, `DOWN`, `LEFT`, `RIGHT` are defined in `main/hardware/k10_input.h`.

## Software Configuration

### Disabling Inputs
To ignore all onboard button inputs (e.g., during hardware debugging or when using external controls), set the following macro in `main/config/k10_config.h`:

```cpp
#define K10_DISABLE_ONBOARD_BUTTONS 1
```

### Input Processing Flow
1.  `k10_hardware.cpp`: `k10_read_inputs()` reads the XL9535 expander via I2C.
2.  `main.cpp`: The main loop calls `k10_read_inputs()` and passes the bitmask to the state handler and emulator.
3.  `k10_state.cpp`: Handles high-level events (Menu navigation, Game Start, Return to Menu).
4.  `k10_emulator.cpp`: Passes the bitmask into the arcade core (e.g., `galaga.h`).

## Implementation Details

The XL9535 pins are active-low (they read `0` when pressed). The hardware layer in `k10_hardware.cpp` inverts this logic so that the `input_states` bitmask uses high bits for "pressed" states.

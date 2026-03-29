# XL9535 IO Expander on the K10 Basic Board

The XL9535 is a 16-bit I2C-bus and SMBus I/O expander, which is commonly used to provide additional General Purpose Input/Output (GPIO) pins for microcontrollers when the native pins are limited. It communicates with the microcontroller (in this case, the K10 Basic Board's ESP32-S3 or similar MCU) via the I2C serial interface.

## Integration with K10 Basic Board

The `src/main.cpp` code confirms that an XL9535 IO Expander is integrated into the K10 Basic Board.
*   **I2C Address:** The default I2C address for the XL9535 on this board is `0x20`.
*   **Communication Protocol:** I2C (Inter-Integrated Circuit)

## How it's Connected (Conceptual)

Although a physical wiring diagram isn't provided, the I2C protocol implies the following connections from the K10 Basic Board's microcontroller to the XL9535:

*   **SDA (Serial Data Line):** For transmitting and receiving data.
*   **SCL (Serial Clock Line):** For synchronizing data transfer.
*   **VCC:** Power supply (typically 3.3V or 5V, depending on the board).
*   **GND:** Ground.

These connections are internal to the K10 Basic Board. You will primarily interact with the XL9535 via its I2C address.

## Accessing and Controlling the XL9535

To interact with the XL9535, you will use the `Wire.h` library, which is the standard Arduino library for I2C communication.

### XL9535 Register Map (K10 Specifics and Common Assumptions)

The XL9535 typically has several registers that control its behavior. Based on the UniHiker K10 Hardware Reference and common XL9535 implementations, these registers are used:

*   **Input Port Register 0 (0x00):** Reads the current state of pins P0.0 - P0.7.
*   **Input Port Register 1 (0x01):** Reads the current state of pins P1.0 - P1.7.
*   **Output Port Register 0 (0x02):** Writes data to the output pins P0.0 - P0.7.
*   **Output Port Register 1 (0x03):** Writes data to the output pins P1.0 - P1.7.
*   **Configuration Register 0 (0x06):** Configures each pin of Port 0 as either an input (1) or an output (0).
*   **Configuration Register 1 (0x07):** Configures each pin of Port 1 as either an input (1) or an output (0).

**Backlight Brightness:** The backlight, controlled by XL9535 P00, is for ON/OFF switching only. Based on the hardware reference, there is no PWM control for variable brightness; it is fixed at full intensity when ON.

**Button Mappings on K10 Basic:**
*   **Key B (Button B):** Mapped to **XL9535 P02** (Port 0, Bit 2).
*   **Key A (Button A):** Mapped to **XL9535 P14** (Port 1, Bit 4).
*   **UserLed:** Mapped to **XL9535 P17** (Port 1, Bit 7).

### Basic I2C Communication with `Wire.h`

Here's how to communicate with the XL9535 using the `Wire` library to set up and read buttons.

#### 1. Initialize I2C

Always start by initializing the I2C bus in your `setup()` function. It's also crucial to configure the XL9535 pins as inputs or outputs as needed.

```cpp
#include <Arduino.h>
#include <Wire.h>

const int XL9535_ADDRESS = 0x20; // I2C address of the XL9535

// XL9535 Register Definitions (Commonly used by K10)
#define XL9535_REG_INPUT_PORT_0     0x00
#define XL9535_REG_INPUT_PORT_1     0x01
#define XL9535_REG_OUTPUT_PORT_0    0x02
#define XL9535_REG_OUTPUT_PORT_1    0x03
#define XL9535_REG_CONFIG_PORT_0    0x06
#define XL9535_REG_CONFIG_PORT_1    0x07

// Helper function to write to an XL9535 register
void writeXL9535Register(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(XL9535_ADDRESS);
  Wire.write(reg);   // Register address
  Wire.write(value); // Data byte
  Wire.endTransmission();
}

// Helper function to read from an XL9535 register
uint8_t readXL9535Register(uint8_t reg) {
  Wire.beginTransmission(XL9535_ADDRESS);
  Wire.write(reg); // Request to read from this register
  Wire.endTransmission();

  Wire.requestFrom(XL9535_ADDRESS, 1); // Request 1 byte of data
  if (Wire.available()) {
    return Wire.read();
  }
  return 0; // Return 0 or handle error
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial time to initialize
  Serial.println("\n--- XL9535 Example ---");
  Wire.begin(); // Join the I2C bus as a master
  Serial.println("[INFO] I2C bus initialized.");

  // Configure XL9535 Pins:
  // For Configuration Registers (0x06, 0x07):
  //   '1' = Input (default)
  //   '0' = Output

  // Configure Port 0: P02 (Key B) as Input (1), others as inputs too
  // All bits '1' (0xFF) means all P0 pins are inputs.
  writeXL9535Register(XL9535_REG_CONFIG_PORT_0, 0xFF); 
  Serial.println("[INFO] XL9535 Port 0 configured for inputs.");

  // Configure Port 1: P14 (Key A) as Input (1), P17 (UserLed) as Output (0), others as inputs
  // 0x7F = 0b01111111 (Bit 7 (P17) is Output, others are Input)
  writeXL9535Register(XL9535_REG_CONFIG_PORT_1, 0x7F); 
  Serial.println("[INFO] XL9535 Port 1 configured for buttons and LED.");

  Serial.println("--- Setup Complete ---\n");
}
```

#### 2. Reading Buttons and Controlling LED in `loop()`

This example demonstrates how to read the state of Button A and Button B, and also how to toggle the UserLed. It uses state-change detection (simple debouncing) to only report button presses once.

```cpp
// Variables for button state tracking (for debouncing)
static byte lastPort0State = 0xFF; // Assume all inputs high initially
static byte lastPort1State = 0xFF; // Assume all inputs high initially

// Bitmasks for buttons and LED
const byte KEY_B_BIT = (1 << 2); // P02 is Bit 2 of Port 0
const byte KEY_A_BIT = (1 << 4); // P14 is Bit 4 of Port 1
const byte USER_LED_BIT = (1 << 7); // P17 is Bit 7 of Port 1

void loop() {
  // Read current states of Port 0 and Port 1
  byte currentPort0State = readXL9535Register(XL9535_REG_INPUT_PORT_0);
  byte currentPort1State = readXL535Register(XL9535_REG_INPUT_PORT_1);

  // --- Check Key B (Port 0, Bit 2) ---
  // Buttons are typically active LOW, meaning the bit goes LOW when pressed.
  if (!(currentPort0State & KEY_B_BIT) && (lastPort0State & KEY_B_BIT)) {
    Serial.println("[EVENT] Key B (Button B) Pressed!");
    
    // Example: Toggle UserLed on Key B press
    static bool ledState = false;
    ledState = !ledState;
    
    // Read current output register state for Port 1 first to avoid disturbing other pins
    uint8_t port1Output = readXL9535Register(XL9535_REG_OUTPUT_PORT_1);
    if (ledState) {
        port1Output |= USER_LED_BIT; // Set P17 HIGH
    } else {
        port1Output &= ~USER_LED_BIT; // Set P17 LOW
    }
    writeXL9535Register(XL9535_REG_OUTPUT_PORT_1, port1Output);
    Serial.print("[ACTION] UserLed Toggled: ");
    Serial.println(ledState ? "ON" : "OFF");
  }

  // --- Check Key A (Port 1, Bit 4) ---
  if (!(currentPort1State & KEY_A_BIT) && (lastPort1State & KEY_A_BIT)) {
    Serial.println("[EVENT] Key A (Button A) Pressed!");
    // You can add other actions here for Button A
  }

  // Update last states for next iteration
  lastPort0State = currentPort0State;
  lastPort1State = currentPort1State;

  delay(20); // Small delay to prevent excessive I2C traffic and help with debouncing
}
```

**Important Notes:**

*   **Always Consult the Datasheet:** While this guide uses information from the K10 hardware reference and common XL9535 patterns, always refer to the official XL9535 datasheet for the most accurate and complete information regarding register addresses, bit meanings, and advanced features.
*   **Bit Manipulation:** When controlling individual pins or groups of pins, you'll use bitwise operations (AND, OR, SHIFT) to set or clear specific bits within the 8-bit or 16-bit register values.
*   **Error Handling:** The examples are simplified. In a robust application, you should add error checking for I2C transmissions (e.g., checking the return value of `Wire.endTransmission()`)
*   **Interrupts:** The UniHiker K10 Hardware Reference mentions that the XL9535 generates a `BUS_INT` interrupt to the ESP32 on input events. For more responsive button handling, consider implementing an interrupt service routine (ISR) on the ESP32 to detect this interrupt and then read the XL9535, rather than continuous polling in the `loop()`. This is more advanced but highly recommended for battery-powered or low-latency applications.

This updated Markdown file provides a clear guide on interacting with the XL9535 for button input and LED control on your K10 Basic Board.

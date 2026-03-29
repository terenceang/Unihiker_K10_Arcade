# UniHiker K10 — Hardware Reference & Signal Flow Guide

**DFR0992 · ESP32-S3-WROOM-1 · Revision V1.0**

---

## 1. System Overview

The UniHiker K10 (DFR0992) is a high-performance AI and IoT development platform centred on the ESP32-S3-WROOM-1 module. It integrates display, camera, audio, RGB LEDs, buttons, and a Micro:bit-compatible edge connector via GPIO expansion. The board uses a **Hub-and-Spoke architecture**: the ESP32-S3 handles high-speed tasks (Display, Camera, Audio), while the XL9535 I/O Expander manages low-speed user interactions and auxiliary power control, offloading GPIO requirements from the main MCU.

### 1.1 Main Functional Blocks

| Block | Interface | Major ICs |
|---|---|---|
| MCU | Native | ESP32-S3-WROOM-1 |
| LCD | SPI | ILI9341 |
| Camera | DVP parallel | GC2145 |
| Audio Codec (mic) | I2S + I2C | ES7243EU8 |
| Audio Amplifier | I2S | NS4168 |
| MEMS Microphones (×2) | Analog → Codec | MSM381ACT001 |
| IO Expander | I2C | XL9535QF24 |
| RGB LEDs | GPIO | WS2812-type |
| Edge Connector | GPIO via expander | Micro:bit compatible |
| USB | Native USB | ESP32-S3 |

---

## 2. Main MCU — ESP32-S3-WROOM-1 (U4)

### 2.1 Power & Reset

| Pin | Net |
|---|---|
| 3V3 | 3.3 V rail |
| EN | Reset pull-up |
| GND | Ground |

### 2.2 USB

| ESP32 Pin | Net |
|---|---|
| GPIO19 | USB_D− |
| GPIO20 | USB_D+ |

### 2.3 Boot / UART

| ESP32 Pin | Net |
|---|---|
| GPIO0 | BOOT button |
| GPIO43 | TXD0 |
| GPIO44 | RXD0 |

### 2.4 Strapping Pins

| GPIO | Default |
|---|---|
| 0 | Pull UP |
| 3 | N/A |
| 45 | Pull Down |
| 46 | Pull Down |

---

## 3. System Buses

### 3.1 I2C Bus (Primary)

Used by: IO Expander (XL9535), Audio Codec (ES7243), I2C Sensors, and External Headers.

| Signal | ESP32 Pin |
|---|---|
| I2C_SDA | GPIO47 → P20/SDA |
| I2C_SCL | GPIO48 → P19/SCL |

### 3.2 I2C Scan Results (Standard Components)

| Address | Major IC | Function | Notes |
|---|---|---|---|
| 0x11 | ES7243EU8 | Audio Codec | Microphone interface & processing |
| 0x19 | SC7A20H | 3-axis Accelerometer | Motion and orientation sensing |
| 0x20 | XL9535QF24 | IO Expander | LEDs, LCD Backlight, Front Buttons |
| 0x29 | LTR-303ALS-01 | Ambient Light Sensor | Illuminance measurement |
| 0x38 | AHT20 | Temp & Humidity | Environmental sensing |

### 3.3 SPI Bus (LCD)

| Signal | Net / ESP32 Pin |
|---|---|
| LCD_SCLK | GPIO12 (`SCK`) |
| LCD_MOSI | GPIO21 (`MOSI`) |
| LCD_CS | GPIO14 |
| LCD_DC | GPIO13 |
| LCD_RST | Not driven by a dedicated GPIO in the Arduino board package (`TFT_RST = -1`) |
| LCD_BLK | Backlight control via XL9535 P00 |

### 3.3 I2S Bus (Audio)

| Signal | Function |
|---|---|
| I2S_BLCK | Bit clock |
| I2S_LRCK | LR clock |
| I2S_MCLK | Master clock |
| I2S_SDI / I2S_ADCDAT | Microphone data (Codec → ESP32) |
| I2S_SDO | Speaker data (ESP32 → Amplifier) |

---

## 4. LCD Subsystem — ILI9341

The ILI9341 is a SPI TFT controller driven directly by ESP32-S3 GPIOs for low-latency command processing. Backlight is driven by an MMBT3904 NPN transistor (Q4), switched by XL9535 pin P00.

| LCD Pin | Net |
|---|---|
| SCL | LCD_SCLK (GPIO12) |
| SDI | LCD_MOSI (GPIO21) |
| CS | LCD_CS (GPIO14) |
| DC | LCD_DC (GPIO13) |
| RST | LCD_RST (not mapped to a dedicated Arduino GPIO in the installed `unihiker_k10` board package) |
| LED+ | Backlight (via MMBT3904 transistor) |
| LED− | GND |

> **Backlight signal path:** ESP32-S3 → XL9535 (P00, I2C) → MMBT3904 base → LCD LED+
>
> **Board support note:** The installed `unihiker_k10` Arduino variant defines `SDA=47`, `SCL=48`, `MOSI=21`, and `SCK=12`. The working LCD configuration validated in firmware uses `CS=14`, `DC=13`, and `RST=-1`.

---

## 5. Camera Subsystem — GC2145 (DVP)

The GC2145 uses the DVP (Digital Video Port) parallel interface. The camera is powered by a dedicated 2.8 V rail (U6 BL8555-28PRA) with a separate 1.8 V supply (U7 AP7343Q-18W5-7). Camera reset is controlled via XL9535 pin P01.

| Signal | Direction | Description |
|---|---|---|
| Camera_XCLK | ESP32 → Cam | Master clock provided by ESP32 |
| Camera_VSYNC | Cam → ESP32 | Frame sync |
| Camera_HREF / HSYNC | Cam → ESP32 | Line sync |
| Camera_PCLK | Cam → ESP32 | Pixel clock |
| Camera_D2 … D9 | Cam → ESP32 | 8-bit parallel data (DMA) |
| Camera_RST | XL9535 → Cam | Reset via IO Expander P01 |

> Data is transferred 8 bits per pixel clock cycle, directly into ESP32 internal memory via DMA.

---

## 6. Audio Subsystem

### 6.1 Audio Codec — ES7243EU8 (U8)

I2S ADC converting analog microphone signals to digital audio. Controlled via I2C (address: `0x10[AD2][AD1][AD0]`).

| Signal | Net |
|---|---|
| SCLK | I2S_BLCK |
| LRCK | I2S_LRCK |
| MCLK | I2S_MCLK |
| SDOUT / AD2 | I2S_ADCDAT |
| I2C SCL/SDA | P19/SCL, P20/SDA (control bus) |

### 6.2 MEMS Microphones — MSM381ACT001 (U9, U10)

Dual stereo MEMS microphones feed differential analog audio into the ES7243 codec.

| Mic | Connection |
|---|---|
| Mic L (U10) | AINLP / AINLN |
| Mic R (U9) | AINRP / AINRN |

### 6.3 Speaker Amplifier — NS4168 (U11)

I2S digital audio amplifier. Drives the onboard speaker via differential output VO+ / VO−.

| Pin | Net |
|---|---|
| LRCLK | I2S_LRCK |
| BCLK | I2S_BLCK |
| SDATA | I2S_SDI |
| VO+ / VO− | Speaker output |

### 6.4 Audio Signal Chain

- **Input:** MSM381 mics (L+R analog) → ES7243 codec → I2S_ADCDAT → ESP32-S3
- **Processing:** ESP32-S3 (e.g. AI voice recognition)
- **Output:** ESP32-S3 → I2S_SDI → NS4168 amplifier → Speaker

---

## 7. IO Expansion — XL9535QF24 (U5)

The XL9535 is a 16-bit I2C GPIO expander at address `0x20` (7-bit address `0b0100000`). It offloads GPIO management for low-speed peripherals and generates a BUS_INT interrupt to the ESP32 on input events.

| Expander Pin | Net / Function |
|---|---|
| P00 | LCD_BLK (backlight control) |
| P01 | Camera_RST |
| P02 | P11/KeyB |
| P03 | P12 |
| P04 | P13 |
| P05 | P14 |
| P06 | P15 |
| P07 | P2 |
| P10 | P8 |
| P11 | P9 |
| P12 | P10 |
| P13 | P6 |
| P14 | P5/KeyA |
| P15 | P4 |
| P16 | P3 |
| P17 | UserLed |
| INT | BUS_INT → ESP32 |
| SCL / SDA | I2C bus (P19/P20) |

---

## 8. Micro:bit Edge Connector (J9A)

The Micro:bit-compatible edge connector exposes GPIO, analog, SPI, I2C, and special-function pins. Digital pins P3 and P8–P16 are routed through the XL9535 IO Expander.

| Edge Pin | Signal / Function |
|---|---|
| P0 | Analog / GPIO |
| P1 | Analog / GPIO |
| P2 | Analog / GPIO |
| P3 | GPIO (via XL9535) |
| P4 | Light sensor / Analog |
| P5 | Button A (KeyA) |
| P6 | Buzzer |
| P7 | NeoPixel (RGB) |
| P8 | GPIO (via XL9535) |
| P9 | GPIO (via XL9535) |
| P10 | Sound / Analog |
| P11 | Button B (KeyB) |
| P12 | GPIO (via XL9535) |
| P13 | SPI SCK |
| P14 | SPI MISO |
| P15 | SPI MOSI |
| P16 | GPIO (via XL9535) |
| P19 | I2C SCL |
| P20 | I2C SDA |

---

## 9. Buttons & RGB LEDs

### 9.1 Buttons

| Button | Net | Notes |
|---|---|---|
| BOOT (K1) | GPIO0 | Strapping pin, Pull UP |
| RST (K2) | EN | Hardware reset |
| Key A (K3) | P5/KeyA | Via XL9535 P14; triggers BUS_INT |
| Key B (K4) | P11/KeyB | Via XL9535 P02; triggers BUS_INT |

### 9.2 RGB LEDs

WS2812-type daisy-chain LEDs (RGB1, RGB2, RGB3 + one spare). All driven from the RGB signal net. A UserLed indicator is also present, controlled via XL9535 P17.

| Signal | Net |
|---|---|
| DIN | RGB |

---

## 10. Onboard I2C Sensors

| IC | Component | I2C Address | Function |
|---|---|---|---|
| U1 | AHT20 | 0x38 | Temperature & Humidity |
| U2 | LTR-303ALS-01 | 0x29 | Ambient Light |
| U3 | SC7A20H | 0x19 | Accelerometer (3-axis) |
| U5 | XL9535QF24 | 0x20 | GPIO Expander |
| U8 | ES7243EU8 | 0x10+ | Audio Codec (I2C control) |

---

## 11. External Headers

### 11.1 I2C Header (J2)

| Pin | Signal |
|---|---|
| 1 | 3V3 |
| 2 | SCL |
| 3 | SDA |
| 4 | GND |

### 11.2 GPIO Header (J5)

| Pin | Signal |
|---|---|
| 1 | 3V3 |
| 2 | P0 |
| 3 | P1 |

### 11.3 I2C Header (J4)

| Pin | Signal |
|---|---|
| 1 | P0 |
| 2 | 3V3 |
| 3 | GND |

---

## 12. TF Card & Font Chip

### 12.1 MicroSD (M2)

The MicroSD card uses the SPI3 bus (`CS3`, `MOSI3`, `SCLK3`, `MISO3`). A MMBT3904T transistor (Q1) is used for level/switch control of CS3.

### 12.2 Font Chip — GT30L24A3W (U15)

A dedicated SPI font ROM chip shares the SPI3 bus (`CS3#`, `MOSI3`, `SCLK3`, `MISO3`).

> **Important:** CS3 is shared between MicroSD and Camera_D2 (see Shared Pin Constraints below).

---

## 13. Power Management & Startup

### 13.1 Power Rails

| Rail | Source | Consumers |
|---|---|---|
| VUSB | USB connector / Battery | System input |
| 3.3 V | LDO regulator | ESP32-S3, XL9535, most ICs |
| 2.8 V | U6 BL8555-28PRA | GC2145 camera AVDD |
| 1.8 V | U7 AP7343Q-18W5-7 | GC2145 camera DOVDD |
| 1.2 V | Internal to GC2145 | GC2145 DVDD |

### 13.2 Startup Sequence

1. USB or Battery power enters the system; 3.3 V LDO powers ESP32-S3 and XL9535.
2. ESP32-S3 initialises the I2C bus and instructs the XL9535 to enable Camera Power (if required) and turn on the LCD Backlight.
3. ESP32-S3 initialises I2C sensors (AHT20, LTR-303, SC7A20H) and SPI/I2S peripherals.

---

## 14. Hardware Interaction & Signal Flow

### 14.1 Display & Visual Output

- ESP32-S3 sends pixel data over the dedicated SPI bus to the ILI9341.
- `LCD_CS`, `LCD_DC`, and `LCD_RST` are toggled directly by ESP32-S3 GPIOs.
- **Backlight:** ESP32 writes to XL9535 P00 (via I2C) → XL9535 P00 drives the MMBT3904 base → transistor switches LCD LED+ on.

### 14.2 User Input (Buttons & Edge Connector)

- When Button A (XL9535 P14) or Button B (XL9535 P02) is pressed, the XL9535 pulls `BUS_INT` low.
- ESP32-S3 detects the interrupt, then reads XL9535 registers via I2C to determine which button was pressed.
- Edge connector digital pins (P3, P8–P16) are routed through the XL9535, so the ESP32 can interact with external modules without consuming native high-speed GPIO pins.

### 14.3 Audio Signal Chain

- **Input:** MSM381 MEMS mics capture stereo analog audio → ES7243 codec digitises it → sent to ESP32 via I2S (`I2S_ADCDAT`).
- **Processing:** ESP32-S3 handles the audio (e.g. AI voice recognition).
- **Output:** ESP32-S3 → `I2S_SDI` → NS4168 amplifier → drives the onboard speaker.

### 14.4 Camera Imaging

- ESP32 provides the master clock (`Camera_XCLK`) to the GC2145.
- Camera returns `Camera_VSYNC` (frame sync) and `Camera_HREF` (line sync) to coordinate data transfer.
- 8-bit data (`Camera_D2`–`D9`) is transferred per pixel clock directly into ESP32 internal memory via DMA.
- Camera power is enabled via the 2.8 V rail; reset is controlled by XL9535 P01.

---

## 15. Shared Pin Constraints

Because the ESP32-S3 has a finite number of GPIOs, several signal paths are shared. These constraints must be observed in firmware:

| Shared Resource | Functions | Constraint |
|---|---|---|
| Pin 40 / CS3 | MicroSD CS and Camera_D2 | SD card transactions must complete and CS must be released before the camera can use this pin. |
| Pins 42 & 45 | I2S Clocks (I2S_BLCK, I2S_LRCK) and Camera Data lines | Simultaneous high-quality audio recording and camera streaming is not possible. |

---

## 16. Interrupt Lines

| Signal | Source | Description |
|---|---|---|
| BUS_INT | XL9535 IO Expander (INT pin) | Asserted low when any expander input changes (button press, edge connector event). ESP32 reads XL9535 registers via I2C to identify source. |

---

*Document compiled from HARDWARE_OVERVIEW.md, HARDWARE_INTERACTIONS.md, and UniHiker K10 schematic (DFR0992, Rev V1.0, 2025-02-13). All pin assignments verified against schematic pages 1–11.*

# Adafruit 3.2" TFT Display Wiring for ESP32-S3 DevKitC

## Hardware Configuration

Display uses SPI3, W5500 Ethernet uses SPI2. Separate buses required due to CS handling differences.

## Pin Assignments

### W5500 Ethernet (SPI2)

| Signal | ESP32-S3 GPIO | Notes |
|--------|---------------|-------|
| SCLK   | GPIO12        | SPI2 default |
| MOSI   | GPIO11        | SPI2 default |
| MISO   | GPIO13        | SPI2 default |
| CS     | GPIO10        | Hardware CS |
| INT    | GPIO4         | Directly on W5500 node |
| RESET  | GPIO5         | Directly on W5500 node |

### ILI9341 Display (SPI3)

| Signal | ESP32-S3 GPIO | Notes |
|--------|---------------|-------|
| SCLK   | GPIO36        | SPI3 default |
| MOSI   | GPIO39        | SPI3 default |
| MISO   | GPIO37        | SPI3 default (optional, display is write-only) |
| CS     | GPIO15        | Software CS via cs-gpios |
| DC     | GPIO14        | Data/Command select |
| RESET  | GPIO21        | Display reset |

## Wiring Diagram

```
ESP32-S3 DevKitC              Adafruit 3.2" TFT
==================            =================
3.3V  ──────────────────────► Vin
GND   ──────────────────────► GND

GPIO36 (SPI3 SCLK) ─────────► CLK
GPIO39 (SPI3 MOSI) ─────────► MOSI
GPIO15 ─────────────────────► CS
GPIO14 ─────────────────────► D/C
GPIO21 ─────────────────────► RST
3.3V   ─────────────────────► Lite (backlight, always on)
```

## Display Configuration

### SPI Mode Jumpers

Close jumpers on the display back for SPI mode:
- **IM1**: Close
- **IM2**: Close
- **IM3**: Close
- **IM0**: Leave OPEN

### Devicetree Configuration

The display is configured in `boards/esp32s3_devkitc_procpu.overlay`:
- Compatible: `ilitek,ili9341`
- Resolution: 320x240
- Pixel format: RGB565
- SPI frequency: 15 MHz
- Rotation: 90°

### Kconfig Options

Required in `boards/esp32s3_devkitc_procpu.conf`:
```
CONFIG_DISPLAY=y
CONFIG_MIPI_DBI=y
CONFIG_ILI9341=y
CONFIG_HEAP_MEM_POOL_SIZE=16384
```

## Color Mapping Issue

The ILI9341 driver sets BGR mode by default, but the Adafruit display appears to have different color mapping. Observed behavior:

| RGB565 Value | Expected Color | Actual Color |
|--------------|----------------|--------------|
| `0xF800`     | Red            | Blue         |
| `0x07E0`     | Green          | Red          |
| `0x001F`     | Blue           | Green        |

**Workaround**: When drawing, swap color channels:
- For Red: send `0x07E0` (green value)
- For Green: send `0x001F` (blue value)
- For Blue: send `0xF800` (red value)

## Why Separate SPI Buses?

The W5500 Ethernet adapter requires hardware-controlled CS (chip select) to work correctly with the ESP32-S3. When `cs-gpios` is added to the SPI controller for multi-device support, it switches to software-controlled CS which causes W5500 initialization failures ("Unable to read RTR register" error).

Solution: Use SPI2 for W5500 (with hardware CS) and SPI3 for the display (with software CS via `cs-gpios`).

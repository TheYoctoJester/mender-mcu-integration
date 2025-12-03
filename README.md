[![Build Status](https://gitlab.com/Northern.tech/Mender/mender-mcu-integration/badges/main/pipeline.svg)](https://gitlab.com/Northern.tech/Mender/mender-mcu-integration/pipelines)

# mender-mcu-integration

Mender is an open source over-the-air (OTA) software updater for embedded devices. Mender
comprises a client running at the embedded device, as well as a server that manages deployments
across many devices.

Mender provides modules to integrate with Real Time Operating Systems (RTOS). The code can be found
in [`mender-mcu` repository](https://github.com/mendersoftware/mender-mcu/).

This repository contains a reference project on how to integrate a user application with Mender OTA
Zephyr Module, with configurations for some boards to choose from.

-------------------------------------------------------------------------------

![Mender logo](https://github.com/mendersoftware/mender/raw/master/mender_logo.png)


## Get started

To get started with Mender for Microcontrollers, visit [Mender documentation](https://docs.mender.io/get-started/microcontroller-preview).

### ESP32-S3-DevKitC with W5500 Ethernet and ILI9341 Display

This setup uses [ESP32-S3-DevKitC](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide.html#hardware-reference) with [W5500 Ethernet](https://www.reichelt.com/de/en/shop/product/developer_boards_-_spi-ethernet_interface_converter-305766) and [Adafruit 3.2" TFT Display (ILI9341)](https://www.adafruit.com/product/1743).

The W5500 and display use separate SPI buses (SPI2 and SPI3) because W5500 requires hardware CS.

#### MAC Address

The W5500 MAC address is derived at runtime from the ESP32's unique chip ID (efuse). This allows the same firmware image to be used on multiple boards, each getting a unique, stable MAC address. The MAC uses Wiznet's OUI (00:08:DC) combined with a CRC32 hash of the chip ID for good distribution.

#### Wiring

**W5500 Ethernet (SPI2):**
| Signal | ESP32-S3 GPIO |
|--------|---------------|
| SCLK   | GPIO12        |
| MOSI   | GPIO11        |
| MISO   | GPIO13        |
| CS     | GPIO10        |
| INT    | GPIO4         |
| RESET  | GPIO5         |
| VCC    | 3.3V          |
| GND    | GND           |

**ILI9341 Display (SPI3):**
| Signal | ESP32-S3 GPIO |
|--------|---------------|
| CLK    | GPIO36        |
| MOSI   | GPIO39        |
| CS     | GPIO15        |
| D/C    | GPIO14        |
| RST    | GPIO21        |
| Vin    | 3.3V          |
| GND    | GND           |
| Lite   | 3.3V          |

See [docs/display-wiring.md](docs/display-wiring.md) for detailed wiring and configuration.

#### Regenerating the Display Logo

The Mender logo displayed on the ILI9341 is stored as RGB565 pixel data in `src/mender_logo.h`. To regenerate it from the source PNG (requires Python 3 with Pillow):

```bash
# Download the high-resolution source logo
curl -LO https://raw.githubusercontent.com/mendersoftware/mender/master/mender_logo.png

# Convert to RGB565 header file
python3 << 'EOF'
from PIL import Image

img = Image.open('mender_logo.png')
target_width = 300
target_height = int(target_width * img.size[1] / img.size[0])

img_resized = img.resize((target_width, target_height), Image.LANCZOS)

if img_resized.mode == 'RGBA':
    background = Image.new('RGB', img_resized.size, (255, 255, 255))
    background.paste(img_resized, mask=img_resized.split()[3])
    img_rgb = background
else:
    img_rgb = img_resized.convert('RGB')

def rgb_to_ili9341(r, g, b):
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    rgb565 = (r5 << 11) | (g6 << 5) | b5
    return ((rgb565 & 0xFF) << 8) | ((rgb565 >> 8) & 0xFF)

pixels = list(img_rgb.getdata())

with open('src/mender_logo.h', 'w') as f:
    f.write(f'''/*
 * Mender logo - RGB565 format (byte-swapped for ILI9341)
 * Size: {target_width}x{target_height}
 * Source: https://github.com/mendersoftware/mender/blob/master/mender_logo.png
 * Auto-generated - do not edit manually
 */

#ifndef MENDER_LOGO_H
#define MENDER_LOGO_H

#include <stdint.h>

#define MENDER_LOGO_WIDTH  {target_width}
#define MENDER_LOGO_HEIGHT {target_height}

static const uint16_t mender_logo_rgb565[{len(pixels)}] = {{
''')
    for i in range(0, len(pixels), 16):
        row = [rgb_to_ili9341(*p) for p in pixels[i:i+16]]
        f.write('    ' + ','.join(f'0x{v:04x}' for v in row) + ',\n')
    f.write('};\n\n#endif /* MENDER_LOGO_H */\n')

print(f"Generated src/mender_logo.h ({target_width}x{target_height})")
EOF
```

#### Build and Flash

```
west build --sysbuild --board esp32s3_devkitc/esp32s3/procpu mender-mcu-integration -- \
  -DCONFIG_MENDER_SERVER_TENANT_TOKEN=\"$TENANT_TOKEN\"
west flash && west espressif monitor
```

### Native simulator

See board support information from [Zephyr Project
docs](https://docs.zephyrproject.org/latest/boards/native/native_sim/doc/index.html).

*WARNING*: this board does not support MCUBoot, so the default "zephyr-image" Update Module is not
compiled in, but rather "noop-update", which does nothing. This is meant to be used for testing purposes only.

To set up networking, run `net-setup.sh` from `/path/to/workspace/tools/net-tools`, and see
instructions in the
[Zephyr documentation](https://docs.zephyrproject.org/latest/connectivity/networking/qemu_setup.html#setting-up-zephyr-and-nat-masquerading-on-host-to-access-internet)
on how to configure the host machine to access internet.

To build the reference project for native_sim, execute:
```
west build --board native_sim mender-mcu-integration
```

Then you can run the binary with:
```
./build/zephyr/zephyr.exe
```

or
```
west build -t run
```

### Using on-premise demo Mender Server

To run the Native Simulator with on-premise Mender Server:

1. Follow instructions from [mender-server](https://github.com/mendersoftware/mender-server) to start the server

2. Copy and convert the demo certificate for docker.mender.io
    ```
    wget https://raw.githubusercontent.com/mendersoftware/mender-server/main/compose/certs/mender.crt
    openssl x509 -in mender.crt -outform der -out mender.der
    ```

3. Add the following Kconfig parameters to your build:
    ```
    CONFIG_MENDER_SERVER_HOST_ON_PREM=y
    CONFIG_MENDER_SERVER_HOST="https://docker.mender.io"
    CONFIG_MENDER_APP_SERVER_HOST_ON_PREM_CERT="mender.der"
    CONFIG_DNS_SERVER_IP_ADDRESSES=y
    CONFIG_DNS_SERVER1="192.0.2.2:15353"
    ```

4. Start networking with  `net-setup.sh`

5. Start the DNS and DHCP servers with `dnsmasq`
    ```
    cat << EOF > dnsmasq.conf
    # Config for Mender Demo Server DNS and DHCPv4
    interface=zeth

    port=15353
    bind-interfaces
    no-resolv

    address=/docker.mender.io/192.0.2.2
    dhcp-range=192.0.2.16,192.0.2.32,1h
    EOF
    sudo dnsmasq -C dnsmasq.conf -d
    ```

6. Launch the Mender MCU client \o/

    ```
    west build -t run
    ```
## Contributing

We welcome and ask for your contribution. If you would like to contribute to
Mender, please read our guide on how to best get started
[contributing code or documentation](https://github.com/mendersoftware/mender/blob/master/CONTRIBUTING.md).


## License

Mender is licensed under the Apache License, Version 2.0. See
[LICENSE](https://github.com/mendersoftware/mender-mcu-integration/blob/master/LICENSE)
for the full license text.


## Security disclosure

We take security very seriously. If you come across any issue regarding
security, please disclose the information by sending an email to
[security@mender.io](security@mender.io). Please do not create a new public
issue. We thank you in advance for your cooperation.


## Connect with us

* Join the [Mender Hub discussion forum](https://hub.mender.io)
* Follow us on [Twitter](https://twitter.com/mender_io). Please
  feel free to tweet us questions.
* Fork us on [Github](https://github.com/mendersoftware)
* Create an issue in the [bugtracker](https://northerntech.atlassian.net/projects/MEN)
* Email us at [contact@mender.io](mailto:contact@mender.io)
* Connect to the [#mender IRC channel on Libera](https://web.libera.chat/?#mender)


## Authors

Mender was created by the team at [Northern.tech AS](https://northern.tech),
with many contributions from the community. Thanks
[everyone](https://github.com/mendersoftware/mender/graphs/contributors)!

[Mender](https://mender.io) is sponsored by [Northern.tech AS](https://northern.tech).

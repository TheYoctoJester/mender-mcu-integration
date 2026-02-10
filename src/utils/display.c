// Copyright 2024 Northern.tech AS
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(mender_app, LOG_LEVEL_DBG);

#include "display.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/display.h>
#include <zephyr/net/net_if.h>
#include <string.h>

#include <mender_logo.h>

/* 5x7 pixel font - covers ASCII 32-126 */
static const uint8_t font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /* 32 (space) */
    {0x00, 0x00, 0x5F, 0x00, 0x00}, /* 33 ! */
    {0x00, 0x07, 0x00, 0x07, 0x00}, /* 34 " */
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, /* 35 # */
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, /* 36 $ */
    {0x23, 0x13, 0x08, 0x64, 0x62}, /* 37 % */
    {0x36, 0x49, 0x55, 0x22, 0x50}, /* 38 & */
    {0x00, 0x05, 0x03, 0x00, 0x00}, /* 39 ' */
    {0x00, 0x1C, 0x22, 0x41, 0x00}, /* 40 ( */
    {0x00, 0x41, 0x22, 0x1C, 0x00}, /* 41 ) */
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, /* 42 * */
    {0x08, 0x08, 0x3E, 0x08, 0x08}, /* 43 + */
    {0x00, 0x50, 0x30, 0x00, 0x00}, /* 44 , */
    {0x08, 0x08, 0x08, 0x08, 0x08}, /* 45 - */
    {0x00, 0x60, 0x60, 0x00, 0x00}, /* 46 . */
    {0x20, 0x10, 0x08, 0x04, 0x02}, /* 47 / */
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 48 0 */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 49 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /* 50 2 */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 51 3 */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /* 52 4 */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /* 53 5 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /* 54 6 */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /* 55 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /* 56 8 */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 57 9 */
    {0x00, 0x36, 0x36, 0x00, 0x00}, /* 58 : */
    {0x00, 0x56, 0x36, 0x00, 0x00}, /* 59 ; */
    {0x00, 0x08, 0x14, 0x22, 0x41}, /* 60 < */
    {0x14, 0x14, 0x14, 0x14, 0x14}, /* 61 = */
    {0x41, 0x22, 0x14, 0x08, 0x00}, /* 62 > */
    {0x02, 0x01, 0x51, 0x09, 0x06}, /* 63 ? */
    {0x32, 0x49, 0x79, 0x41, 0x3E}, /* 64 @ */
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* 65 A */
    {0x7F, 0x49, 0x49, 0x49, 0x36}, /* 66 B */
    {0x3E, 0x41, 0x41, 0x41, 0x22}, /* 67 C */
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, /* 68 D */
    {0x7F, 0x49, 0x49, 0x49, 0x41}, /* 69 E */
    {0x7F, 0x09, 0x09, 0x01, 0x01}, /* 70 F */
    {0x3E, 0x41, 0x41, 0x51, 0x32}, /* 71 G */
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* 72 H */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, /* 73 I */
    {0x20, 0x40, 0x41, 0x3F, 0x01}, /* 74 J */
    {0x7F, 0x08, 0x14, 0x22, 0x41}, /* 75 K */
    {0x7F, 0x40, 0x40, 0x40, 0x40}, /* 76 L */
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, /* 77 M */
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, /* 78 N */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, /* 79 O */
    {0x7F, 0x09, 0x09, 0x09, 0x06}, /* 80 P */
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, /* 81 Q */
    {0x7F, 0x09, 0x19, 0x29, 0x46}, /* 82 R */
    {0x46, 0x49, 0x49, 0x49, 0x31}, /* 83 S */
    {0x01, 0x01, 0x7F, 0x01, 0x01}, /* 84 T */
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, /* 85 U */
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, /* 86 V */
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, /* 87 W */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /* 88 X */
    {0x03, 0x04, 0x78, 0x04, 0x03}, /* 89 Y */
    {0x61, 0x51, 0x49, 0x45, 0x43}, /* 90 Z */
    {0x00, 0x00, 0x7F, 0x41, 0x41}, /* 91 [ */
    {0x02, 0x04, 0x08, 0x10, 0x20}, /* 92 \ */
    {0x41, 0x41, 0x7F, 0x00, 0x00}, /* 93 ] */
    {0x04, 0x02, 0x01, 0x02, 0x04}, /* 94 ^ */
    {0x40, 0x40, 0x40, 0x40, 0x40}, /* 95 _ */
    {0x00, 0x01, 0x02, 0x04, 0x00}, /* 96 ` */
    {0x20, 0x54, 0x54, 0x54, 0x78}, /* 97 a */
    {0x7F, 0x48, 0x44, 0x44, 0x38}, /* 98 b */
    {0x38, 0x44, 0x44, 0x44, 0x20}, /* 99 c */
    {0x38, 0x44, 0x44, 0x48, 0x7F}, /* 100 d */
    {0x38, 0x54, 0x54, 0x54, 0x18}, /* 101 e */
    {0x08, 0x7E, 0x09, 0x01, 0x02}, /* 102 f */
    {0x08, 0x14, 0x54, 0x54, 0x3C}, /* 103 g */
    {0x7F, 0x08, 0x04, 0x04, 0x78}, /* 104 h */
    {0x00, 0x44, 0x7D, 0x40, 0x00}, /* 105 i */
    {0x20, 0x40, 0x44, 0x3D, 0x00}, /* 106 j */
    {0x00, 0x7F, 0x10, 0x28, 0x44}, /* 107 k */
    {0x00, 0x41, 0x7F, 0x40, 0x00}, /* 108 l */
    {0x7C, 0x04, 0x18, 0x04, 0x78}, /* 109 m */
    {0x7C, 0x08, 0x04, 0x04, 0x78}, /* 110 n */
    {0x38, 0x44, 0x44, 0x44, 0x38}, /* 111 o */
    {0x7C, 0x14, 0x14, 0x14, 0x08}, /* 112 p */
    {0x08, 0x14, 0x14, 0x18, 0x7C}, /* 113 q */
    {0x7C, 0x08, 0x04, 0x04, 0x08}, /* 114 r */
    {0x48, 0x54, 0x54, 0x54, 0x20}, /* 115 s */
    {0x04, 0x3F, 0x44, 0x40, 0x20}, /* 116 t */
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, /* 117 u */
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, /* 118 v */
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, /* 119 w */
    {0x44, 0x28, 0x10, 0x28, 0x44}, /* 120 x */
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, /* 121 y */
    {0x44, 0x64, 0x54, 0x4C, 0x44}, /* 122 z */
    {0x00, 0x08, 0x36, 0x41, 0x00}, /* 123 { */
    {0x00, 0x00, 0x7F, 0x00, 0x00}, /* 124 | */
    {0x00, 0x41, 0x36, 0x08, 0x00}, /* 125 } */
    {0x08, 0x08, 0x2A, 0x1C, 0x08}, /* 126 ~ (arrow) */
};

static const struct device *display_dev;
static struct display_capabilities caps;

/* Row buffer for text rendering - max 320 pixels wide */
static uint16_t row_buffer[320];

/* Render a text string into row_buffer at the given x offset for one font row.
 * Pixels that are "on" are set to fg; background pixels are left untouched
 * (caller pre-fills with background color). */
static void render_text_row(uint16_t x, const char *str, uint16_t font_row, uint16_t fg)
{
    while (*str) {
        if (x + FONT_WIDTH > caps.x_resolution) {
            break;
        }
        char c = *str;
        if (c < 32 || c > 126) {
            c = '?';
        }
        const uint8_t *glyph = font_5x7[c - 32];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if ((glyph[col] >> font_row) & 0x01) {
                row_buffer[x + col] = fg;
            }
        }
        x += CHAR_WIDTH;
        str++;
    }
}

int display_init(void)
{
    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return -ENODEV;
    }

    LOG_INF("Display device: %s", display_dev->name);

    display_get_capabilities(display_dev, &caps);
    LOG_INF("Display: %dx%d, pixel format: %d",
            caps.x_resolution, caps.y_resolution,
            caps.current_pixel_format);

    return 0;
}

void display_logo(void)
{
    if (!device_is_ready(display_dev)) {
        return;
    }

    struct display_buffer_descriptor desc;

    uint16_t footer_y = caps.y_resolution - FOOTER_HEIGHT;

    /* Fill logo area with white */
    for (size_t i = 0; i < caps.x_resolution && i < 320; i++) {
        row_buffer[i] = COLOR_WHITE;
    }
    desc.buf_size = caps.x_resolution * 2;
    desc.pitch = caps.x_resolution;
    desc.width = caps.x_resolution;
    desc.height = 1;

    for (size_t y = 0; y < footer_y; y++) {
        display_write(display_dev, 0, y, &desc, row_buffer);
    }

    /* Fill footer area with teal */
    for (size_t i = 0; i < caps.x_resolution && i < 320; i++) {
        row_buffer[i] = COLOR_TEAL;
    }
    for (size_t y = footer_y; y < caps.y_resolution; y++) {
        display_write(display_dev, 0, y, &desc, row_buffer);
    }

    /* Calculate centered position for logo */
    size_t x_offset = (caps.x_resolution - MENDER_LOGO_WIDTH) / 2;
    size_t y_offset = (caps.y_resolution - FOOTER_HEIGHT - MENDER_LOGO_HEIGHT) / 2;

    /* Draw logo */
    desc.buf_size = MENDER_LOGO_WIDTH * MENDER_LOGO_HEIGHT * 2;
    desc.pitch = MENDER_LOGO_WIDTH;
    desc.width = MENDER_LOGO_WIDTH;
    desc.height = MENDER_LOGO_HEIGHT;

    display_write(display_dev, x_offset, y_offset, &desc, mender_logo_rgb565);

    display_blanking_off(display_dev);
    LOG_INF("Mender logo displayed");
}

/* Small left margin within each column */
#define COL_PADDING 4

void display_update_footer(const char *version, const char *ip_addr, const char *state)
{
    if (!device_is_ready(display_dev)) {
        return;
    }

    uint16_t footer_y = caps.y_resolution - FOOTER_HEIGHT;
    uint16_t text_row_start = (FOOTER_HEIGHT - FONT_HEIGHT) / 2;
    uint16_t col_width = caps.x_resolution / 3;

    const char *ver_str = version ? version : "?";
    const char *ip_str = ip_addr ? ip_addr : "No IP";
    const char *state_str = state ? state : "?";

    uint16_t ver_x = COL_PADDING;
    uint16_t ip_x = col_width + COL_PADDING;
    uint16_t state_x = col_width * 2 + COL_PADDING;

    struct display_buffer_descriptor desc = {
        .buf_size = caps.x_resolution * 2,
        .pitch = caps.x_resolution,
        .width = caps.x_resolution,
        .height = 1,
    };

    for (uint16_t row = 0; row < FOOTER_HEIGHT; row++) {
        for (size_t i = 0; i < caps.x_resolution && i < 320; i++) {
            row_buffer[i] = COLOR_TEAL;
        }

        if (row >= text_row_start && row < text_row_start + FONT_HEIGHT) {
            uint16_t font_row = row - text_row_start;
            render_text_row(ver_x, ver_str, font_row, COLOR_WHITE);
            render_text_row(ip_x, ip_str, font_row, COLOR_WHITE);
            render_text_row(state_x, state_str, font_row, COLOR_WHITE);
        }

        display_write(display_dev, 0, footer_y + row, &desc, row_buffer);
    }
}

int display_get_ip_string(char *buf, size_t buf_len)
{
    struct net_if *iface = net_if_get_default();
    if (!iface) {
        snprintf(buf, buf_len, "No iface");
        return -ENODEV;
    }

    if (!iface->config.ip.ipv4) {
        snprintf(buf, buf_len, "No IP");
        return -ENOENT;
    }

    for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
        if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type != NET_ADDR_DHCP) {
            continue;
        }

        net_addr_ntop(AF_INET,
                      &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
                      buf, buf_len);
        return 0;
    }

    snprintf(buf, buf_len, "No IP");
    return -ENOENT;
}

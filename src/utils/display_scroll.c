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

#include "display_scroll.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_DECLARE(mender_app, LOG_LEVEL_DBG);

#ifdef CONFIG_DISPLAY

#include <zephyr/drivers/display.h>
#include <zephyr/version.h>
#include "font8x16.h"

#define SCROLL_STACK_SIZE 2048
#define SCROLL_PRIORITY   12

/* Display layout */
#define DISPLAY_WIDTH     320
#define SCROLL_Y_POS      222  /* Near bottom edge (240 - 16 - 2) */
#define SCROLL_SPEED_MS   50   /* ~20 fps */
#define SCROLL_STEP       2    /* pixels per frame */

/* Colors in RGB565 byte-swapped for ILI9341 */
#define TEXT_COLOR        0x05B8  /* Mender teal #00b8b8 byte-swapped */
#define BG_COLOR          0xFFFF  /* White */

/* Line buffer for one row of text (320 pixels x 16 rows) */
static uint16_t line_buffer[DISPLAY_WIDTH * FONT_HEIGHT];

/**
 * Render a single character to the line buffer at pixel position x.
 * Characters outside the visible area (0-319) are clipped.
 */
static void render_char(int x, char c)
{
    /* Bounds check for character */
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) {
        c = ' ';  /* Replace unprintable with space */
    }

    int char_index = c - FONT_FIRST_CHAR;

    /* Render each row of the character */
    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint8_t font_row = font8x16_data[char_index][row];

        /* Render each pixel in the row */
        for (int col = 0; col < FONT_WIDTH; col++) {
            int px = x + col;

            /* Clip to visible area */
            if (px < 0 || px >= DISPLAY_WIDTH) {
                continue;
            }

            /* Check if pixel is set (bit 7 is leftmost) */
            int set = (font_row >> (7 - col)) & 1;
            line_buffer[row * DISPLAY_WIDTH + px] = set ? TEXT_COLOR : BG_COLOR;
        }
    }
}

/**
 * Clear the line buffer to background color.
 */
static void clear_line_buffer(void)
{
    for (int i = 0; i < DISPLAY_WIDTH * FONT_HEIGHT; i++) {
        line_buffer[i] = BG_COLOR;
    }
}

/**
 * Render visible portion of scrolling text to line buffer.
 * scroll_offset is the pixel offset into the virtual text buffer.
 */
static void render_text(const char *text, int text_len, int scroll_offset)
{
    int text_width = text_len * FONT_WIDTH;

    clear_line_buffer();

    /* Calculate which characters are visible */
    for (int i = 0; i < text_len; i++) {
        int char_x = i * FONT_WIDTH - scroll_offset;

        /* Wrap around for seamless scrolling */
        if (char_x < -FONT_WIDTH) {
            char_x += text_width;
        }

        /* Skip if completely off-screen */
        if (char_x >= DISPLAY_WIDTH || char_x <= -FONT_WIDTH) {
            continue;
        }

        render_char(char_x, text[i]);
    }
}

static void scroll_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready for scroll");
        return;
    }

    /* Build the scroll text */
    char text[128];
    snprintf(text, sizeof(text),
             "     Zephyr %s   |   Build: %s   |   Device: ESP32-S3-DevKitC     ",
             KERNEL_VERSION_STRING, __DATE__);

    int text_len = strlen(text);
    int text_width = text_len * FONT_WIDTH;
    int scroll_offset = 0;

    LOG_INF("Scroll text started: \"%s\" (%d chars, %d px)", text, text_len, text_width);

    struct display_buffer_descriptor desc = {
        .buf_size = sizeof(line_buffer),
        .width = DISPLAY_WIDTH,
        .height = FONT_HEIGHT,
        .pitch = DISPLAY_WIDTH,
    };

    while (1) {
        /* Render current frame */
        render_text(text, text_len, scroll_offset);

        /* Update display */
        display_write(display_dev, 0, SCROLL_Y_POS, &desc, line_buffer);

        /* Advance scroll position */
        scroll_offset = (scroll_offset + SCROLL_STEP) % text_width;

        k_msleep(SCROLL_SPEED_MS);
    }
}

K_THREAD_DEFINE(scroll_tid, SCROLL_STACK_SIZE,
                scroll_thread, NULL, NULL, NULL,
                SCROLL_PRIORITY, 0, 0);

void display_scroll_start(void)
{
    /* Thread auto-starts via K_THREAD_DEFINE */
    LOG_INF("Display scroll thread initialized");
}

#else /* !CONFIG_DISPLAY */

void display_scroll_start(void)
{
    LOG_WRN("No display configured, scroll disabled");
}

#endif /* CONFIG_DISPLAY */

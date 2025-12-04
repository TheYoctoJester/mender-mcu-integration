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

#include "display.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(mender_app, LOG_LEVEL_DBG);

#ifdef CONFIG_DISPLAY

#include <zephyr/drivers/display.h>
#include "logo.h"

static void display_logo(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    struct display_capabilities caps;
    struct display_buffer_descriptor desc;

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return;
    }

    LOG_INF("Display device: %s", display_dev->name);

    display_get_capabilities(display_dev, &caps);
    LOG_INF("Display: %dx%d, pixel format: %d",
            caps.x_resolution, caps.y_resolution,
            caps.current_pixel_format);

    /* Fill background with white - write full rows at a time for speed */
    static uint16_t white_row[320];
    for (size_t i = 0; i < caps.x_resolution && i < 320; i++) {
        white_row[i] = 0xFFFF;
    }
    desc.buf_size = caps.x_resolution * 2;
    desc.pitch = caps.x_resolution;
    desc.width = caps.x_resolution;
    desc.height = 1;

    for (size_t y = 0; y < caps.y_resolution; y++) {
        display_write(display_dev, 0, y, &desc, white_row);
    }

    /* Calculate centered position for logo */
    size_t x_offset = (caps.x_resolution - MENDER_LOGO_WIDTH) / 2;
    size_t y_offset = (caps.y_resolution - MENDER_LOGO_HEIGHT) / 2;

    /* Draw logo line by line */
    desc.buf_size = MENDER_LOGO_WIDTH * 2;
    desc.pitch = MENDER_LOGO_WIDTH;
    desc.width = MENDER_LOGO_WIDTH;
    desc.height = 1;

    for (size_t y = 0; y < MENDER_LOGO_HEIGHT; y++) {
        display_write(display_dev, x_offset, y_offset + y, &desc,
                      &mender_logo_rgb565[y * MENDER_LOGO_WIDTH]);
    }

    display_blanking_off(display_dev);
    LOG_INF("Mender logo displayed");
}

void display_init(void)
{
    display_logo();
}

#else /* !CONFIG_DISPLAY */

void display_init(void)
{
    LOG_WRN("No display configured, display disabled");
}

#endif /* CONFIG_DISPLAY */

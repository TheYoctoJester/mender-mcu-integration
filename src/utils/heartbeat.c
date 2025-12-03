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

#include "heartbeat.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(mender_app, LOG_LEVEL_DBG);

#define STRIP_NODE DT_ALIAS(led_strip)

#if DT_NODE_EXISTS(STRIP_NODE)

#include <zephyr/drivers/led_strip.h>

#define HEARTBEAT_STACK_SIZE 1024
#define HEARTBEAT_PRIORITY   10

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
static struct led_rgb pixel;

static void heartbeat_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    if (!device_is_ready(strip)) {
        LOG_ERR("LED strip device not ready");
        return;
    }

    LOG_INF("Heartbeat LED started on %s", strip->name);

    /* Turn off LED initially */
    pixel.r = 0;
    pixel.g = 0;
    pixel.b = 0;
    led_strip_update_rgb(strip, &pixel, 1);

    /*
     * Heartbeat pattern: double-pulse every ~2 seconds
     * Using dim green (16/255) for minimal distraction
     */
    while (1) {
        /* First pulse on (dim green) */
        pixel.g = 16;
        led_strip_update_rgb(strip, &pixel, 1);
        k_msleep(100);

        /* First pulse off */
        pixel.g = 0;
        led_strip_update_rgb(strip, &pixel, 1);
        k_msleep(100);

        /* Second pulse on (lub-dub) */
        pixel.g = 16;
        led_strip_update_rgb(strip, &pixel, 1);
        k_msleep(100);

        /* Second pulse off */
        pixel.g = 0;
        led_strip_update_rgb(strip, &pixel, 1);

        /* Long pause until next heartbeat */
        k_msleep(1700);
    }
}

K_THREAD_DEFINE(heartbeat_tid, HEARTBEAT_STACK_SIZE,
                heartbeat_thread, NULL, NULL, NULL,
                HEARTBEAT_PRIORITY, 0, 0);

void heartbeat_start(void)
{
    /* Thread auto-starts via K_THREAD_DEFINE */
    LOG_INF("Heartbeat thread initialized");
}

#else /* !DT_NODE_EXISTS(STRIP_NODE) */

void heartbeat_start(void)
{
    LOG_WRN("No LED strip configured, heartbeat disabled");
}

#endif /* DT_NODE_EXISTS(STRIP_NODE) */

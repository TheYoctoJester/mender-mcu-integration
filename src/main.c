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
LOG_MODULE_REGISTER(mender_app, LOG_LEVEL_DBG);

#include "utils/netup.h"
#include "utils/certs.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

#include "mender/client.h"
#include "mender/inventory.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/util.h>

#ifdef CONFIG_DISPLAY
#include <zephyr/drivers/display.h>
#endif

#define SLEEP_TIME_MS 1000

#ifndef EW_VERSION
#define EW_VERSION 1
#endif

#ifdef CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE
#include <mender/zephyr-image-update-module.h>
#endif /* CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE */

#ifdef CONFIG_MENDER_APP_NOOP_UPDATE_MODULE
#include "modules/noop-update-module.h"
#endif /* CONFIG_MENDER_APP_NOOP_UPDATE_MODULE */

static mender_err_t
network_connect_cb(void) {
    LOG_DBG("network_connect_cb");
    return MENDER_OK;
}

static mender_err_t
network_release_cb(void) {
    LOG_DBG("network_release_cb");
    return MENDER_OK;
}

static mender_err_t
deployment_status_cb(mender_deployment_status_t status, char *desc) {
    LOG_DBG("deployment_status_cb: %s", desc);
    return MENDER_OK;
}

static mender_err_t
restart_cb(void) {
    LOG_DBG("restart_cb");

    sys_reboot(SYS_REBOOT_WARM);

    return MENDER_OK;
}

static char              mac_address[18] = { 0 };
static mender_identity_t mender_identity = { .name = "mac", .value = mac_address };

static mender_err_t
get_identity_cb(const mender_identity_t **identity) {
    LOG_DBG("get_identity_cb");
    if (NULL != identity) {
        *identity = &mender_identity;
        return MENDER_OK;
    }
    return MENDER_FAIL;
}

#ifdef CONFIG_DISPLAY
static void test_display(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    struct display_capabilities caps;
    struct display_buffer_descriptor buf_desc;
    uint16_t color;
    uint8_t *buf;

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return;
    }

    LOG_INF("Display device: %s", display_dev->name);

    display_get_capabilities(display_dev, &caps);
    LOG_INF("Display: %dx%d, pixel_format=%d", caps.x_resolution, caps.y_resolution, caps.current_pixel_format);

    /* Fill with red color (RGB565: 0xF800) */
    buf_desc.width = caps.x_resolution;
    buf_desc.height = 10;  /* Draw 10 lines at a time */
    buf_desc.pitch = caps.x_resolution;
    buf_desc.buf_size = buf_desc.width * buf_desc.height * 2; /* RGB565 = 2 bytes */

    buf = k_malloc(buf_desc.buf_size);
    if (!buf) {
        LOG_ERR("Failed to allocate display buffer");
        return;
    }

    /* Red on this display: color mapping is R->B, G->R, B->G, so send green for red */
    color = 0x07E0;
    for (size_t i = 0; i < buf_desc.buf_size / 2; i++) {
        ((uint16_t *)buf)[i] = color;
    }

    LOG_INF("Drawing red rectangle on display...");
    for (int y = 0; y < caps.y_resolution; y += buf_desc.height) {
        display_write(display_dev, 0, y, &buf_desc, buf);
    }

    k_free(buf);

    display_blanking_off(display_dev);
    LOG_INF("Display test complete - should show red screen");
}
#endif

int
main(void) {

#ifdef CONFIG_DISPLAY
    LOG_INF("Testing display BEFORE network init...");
    test_display();
    LOG_INF("Display test done, waiting 3 seconds...");
    k_msleep(3000);
#endif

    LOG_INF("Now initializing network...");
    netup_wait_for_network();

    netup_get_mac_address(mender_identity.value);

    certs_add_credentials();

    LOG_INF("Initializing Mender Client with:");
    LOG_INF("   Device type:   '%s'", CONFIG_MENDER_DEVICE_TYPE);
    LOG_INF("   Identity:      '{\"%s\": \"%s\"}'", mender_identity.name, mender_identity.value);

    /* Initialize mender-client */
    mender_client_config_t    mender_client_config    = { .device_type = NULL, .recommissioning = false };
    mender_client_callbacks_t mender_client_callbacks = { .network_connect        = network_connect_cb,
                                                          .network_release        = network_release_cb,
                                                          .deployment_status      = deployment_status_cb,
                                                          .restart                = restart_cb,
                                                          .get_identity           = get_identity_cb,
                                                          .get_user_provided_keys = NULL };

    if (MENDER_OK != mender_client_init(&mender_client_config, &mender_client_callbacks)) {
        LOG_ERR("Failed to initialize the client");
        goto END;
    }
    LOG_INF("Mender client initialized");

#ifdef CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE
    if (MENDER_OK != mender_zephyr_image_register_update_module()) {
        LOG_ERR("Failed to register the zephyr-image Update Module");
        goto END;
    }
    LOG_INF("Update Module 'zephyr-image' initialized");
#endif /* CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE */

#ifdef CONFIG_MENDER_APP_NOOP_UPDATE_MODULE
    if (MENDER_OK != noop_update_module_register()) {
        LOG_ERR("Failed to register the noop Update Module");
        goto END;
    }
    LOG_INF("Update Module 'noop-update' initialized");
#endif /* CONFIG_MENDER_APP_NOOP_UPDATE_MODULE */

#ifdef CONFIG_MENDER_CLIENT_INVENTORY
    mender_keystore_t inventory[] = {
	    { .name = "event", .value = "Embedded World 2025" },
	    { .name = "version", .value = STRINGIFY(EW_VERSION) },
	    { .name = NULL, .value = NULL } };
    if (MENDER_OK != mender_inventory_set(inventory)) {
        LOG_ERR("Failed to set the inventory");
        goto END;
    }
    LOG_INF("Mender inventory set");
#endif /* CONFIG_MENDER_CLIENT_INVENTORY */

    /* Finally activate mender client */
    if (MENDER_OK != mender_client_activate()) {
        LOG_ERR("Unable to activate the client");
        goto END;
    }
    LOG_INF("Mender client activated and running!");

    while (1) {
		k_msleep(SLEEP_TIME_MS);
    }

END:
	k_sleep(K_FOREVER);

    return 0;
}

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

#include "utils/callbacks.h"
#include "utils/netup.h"
#include "utils/certs.h"
#include "utils/heartbeat.h"
#include "utils/display_scroll.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/version.h>

#include <mender/utils.h>
#include <mender/client.h>
#include <mender/inventory.h>

#ifdef CONFIG_DISPLAY
#include <zephyr/drivers/display.h>
#include "mender_logo.h"
#endif

#ifdef BUILD_INTEGRATION_TESTS
#include "modules/test-update-module.h"
#include "test_definitions.h"
#endif /* BUILD_INTEGRATION_TESTS */

#ifdef CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE
#include <mender/zephyr-image-update-module.h>
#endif /* CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE */

#ifdef CONFIG_MENDER_APP_NOOP_UPDATE_MODULE
#include "modules/noop-update-module.h"
#endif /* CONFIG_MENDER_APP_NOOP_UPDATE_MODULE */

#ifdef CONFIG_MENDER_CLIENT_INVENTORY_DISABLE
#error Mender MCU integration app requires the inventory feature
#endif /* CONFIG_MENDER_CLIENT_INVENTORY_DISABLE */

MENDER_FUNC_WEAK mender_err_t
mender_network_connect_cb(void) {
    LOG_DBG("network_connect_cb");
    return MENDER_OK;
}

MENDER_FUNC_WEAK mender_err_t
mender_network_release_cb(void) {
    LOG_DBG("network_release_cb");
    return MENDER_OK;
}

MENDER_FUNC_WEAK mender_err_t
mender_deployment_status_cb(mender_deployment_status_t status, const char *desc) {
    LOG_DBG("deployment_status_cb: %s", desc);
    return MENDER_OK;
}

MENDER_FUNC_WEAK mender_err_t
mender_restart_cb(void) {
    LOG_DBG("restart_cb");

    sys_reboot(SYS_REBOOT_WARM);

    return MENDER_OK;
}

static char              mac_address[18] = { 0 };
static mender_identity_t mender_identity = { .name = "mac", .value = mac_address };

MENDER_FUNC_WEAK mender_err_t
mender_get_identity_cb(const mender_identity_t **identity) {
    LOG_DBG("get_identity_cb");
    if (NULL != identity) {
        *identity = &mender_identity;
        return MENDER_OK;
    }
    return MENDER_FAIL;
}

static mender_err_t
persistent_inventory_cb(mender_keystore_t **keystore, uint8_t *keystore_len) {
    static mender_keystore_t inventory[] = {
        { .name = "App", .value = "mender-mcu-integration" },
        { .name = "Network", .value = "W5500-Ethernet" },
        { .name = "Display", .value = "ILI9341-320x240" }
    };
    *keystore                            = inventory;
    *keystore_len                        = 3;
    return MENDER_OK;
}

#ifdef CONFIG_DISPLAY
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
#endif

int
main(void) {
    LOG_INF("Mender MCU Integration on Zephyr %s", KERNEL_VERSION_STRING);

    /* Start heartbeat LED early to indicate boot */
    heartbeat_start();

#ifdef CONFIG_DISPLAY
    display_logo();
    display_scroll_start();
#endif

    netup_wait_for_network();

    netup_get_mac_address(mender_identity.value);

    certs_add_credentials();

    /* Initialize mender-client */
    mender_client_config_t    mender_client_config    = { .device_type = CONFIG_MENDER_DEVICE_TYPE, .recommissioning = false };
    mender_client_callbacks_t mender_client_callbacks = { .network_connect        = mender_network_connect_cb,
                                                          .network_release        = mender_network_release_cb,
                                                          .deployment_status      = mender_deployment_status_cb,
                                                          .restart                = mender_restart_cb,
                                                          .get_identity           = mender_get_identity_cb,
                                                          .get_user_provided_keys = NULL };

    LOG_INF("Initializing Mender Client with:");
    LOG_INF("   Device type:   '%s'", mender_client_config.device_type);
    LOG_INF("   Identity:      '{\"%s\": \"%s\"}'", mender_identity.name, mender_identity.value);

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

#ifdef BUILD_INTEGRATION_TESTS
    if (MENDER_OK != test_update_module_register()) {
        LOG_ERR("Failed to register the test Update Module");
        goto END;
    }
    LOG_INF("Update Module 'test-update' initialized");
#endif /* BUILD_INTEGRATION_TESTS */

    if (MENDER_OK != mender_inventory_add_callback(persistent_inventory_cb, true)) {
        LOG_ERR("Failed to add inventory callback");
        goto END;
    }
    LOG_INF("Mender inventory callback added");

    /* Finally activate mender client */
    if (MENDER_OK != mender_client_activate()) {
        LOG_ERR("Unable to activate the client");
        goto END;
    }
    LOG_INF("Mender client activated and running!");

END:
    k_sleep(K_FOREVER);

    return 0;
}

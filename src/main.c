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
#include <zephyr/drivers/led_strip.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/util.h>

// based on https://github.com/nrfconnect/sdk-zephyr/tree/v3.5.99-ncs1-1/samples/drivers/led_ws2812

#define STRIP_NODE		DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS	DT_PROP(DT_ALIAS(led_strip), chain_length)

#define SLEEP_TIME_MS 1000

#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

static const struct led_rgb O = RGB(0x00, 0x00, 0x00);
static const struct led_rgb W = RGB(0xff, 0xff, 0xff);
static const struct led_rgb M = RGB(0x80, 0x80, 0x80);
static const struct led_rgb R = RGB(0x0f, 0x00, 0x00);
static const struct led_rgb G = RGB(0x00, 0x0f, 0x00);
static const struct led_rgb B = RGB(0x00, 0x00, 0x0f);
static const struct led_rgb Y = RGB(0x0f, 0x0f, 0x00);

const struct led_rgb pixels_off[STRIP_NUM_PIXELS] = {
    O, O, O, O, O, O, O, O,
    O, O, O, O, O, O, O, O,
    O, O, O, O, O, O, O, O,
    O, O, O, O, O, O, O, O,
    O, O, O, O, O, O, O, O,
    O, O, O, O, O, O, O, O,
    O, O, O, O, O, O, O, O,
    O, O, O, O, O, O, O, O
};

const struct led_rgb pixels_boot[STRIP_NUM_PIXELS] = {
    O, O, O, O, O, O, O, O,
    O, O, O, W, W, O, O, O,
    O, O, W, O, O, W, O, O,
    O, W, O, O, O, O, W, O,
    O, W, O, O, O, O, W, O,
    O, O, W, O, O, W, O, O,
    O, O, O, W, W, O, O, O,
    O, O, O, O, O, O, O, O
};


#ifndef EW_VERSION
#define EW_VERSION 1
#endif

#if EW_VERSION == 1

const struct led_rgb pixels_payload1[STRIP_NUM_PIXELS] = {
    O, O, Y, Y, Y, Y, O, O,
    O, Y, Y, Y, Y, Y, Y, O,
    Y, Y, R, Y, W, W, Y, Y,
    Y, R, Y, Y, Y, Y, Y, Y,
    Y, R, Y, Y, Y, Y, Y, Y,
    Y, Y, R, Y, W, W, Y, Y,
    O, Y, Y, Y, Y, Y, Y, O,
    O, O, Y, Y, Y, Y, O, O
};

const struct led_rgb pixels_payload2[STRIP_NUM_PIXELS] = {
    O, O, Y, Y, Y, Y, O, O,
    O, Y, Y, Y, Y, Y, Y, O,
    Y, Y, R, Y, W, W, Y, Y,
    Y, R, Y, Y, Y, Y, Y, Y,
    Y, R, Y, Y, Y, Y, Y, Y,
    Y, Y, R, Y, W, W, Y, Y,
    O, Y, Y, Y, Y, Y, Y, O,
    O, O, Y, Y, Y, Y, O, O
};

#else

const struct led_rgb pixels_payload1[STRIP_NUM_PIXELS] = {
    O, O, Y, Y, Y, Y, O, O,
    O, Y, Y, Y, Y, Y, Y, O,
    Y, O, Y, Y, W, W, Y, Y,
    Y, Y, O, Y, Y, Y, Y, Y,
    Y, Y, O, Y, Y, Y, Y, Y,
    Y, O, Y, Y, W, W, Y, Y,
    O, Y, Y, Y, Y, Y, Y, O,
    O, O, Y, Y, Y, Y, O, O
};

const struct led_rgb pixels_payload2[STRIP_NUM_PIXELS] = {
    R, R, Y, Y, Y, Y, R, R,
    R, Y, Y, Y, Y, Y, Y, R,
    Y, O, Y, Y, W, W, Y, Y,
    Y, Y, O, Y, Y, Y, Y, Y,
    Y, Y, O, Y, Y, Y, Y, Y,
    Y, O, Y, Y, W, W, Y, Y,
    R, Y, Y, Y, Y, Y, Y, R,
    R, R, Y, Y, Y, Y, R, R
};

#endif

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

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

static void set_leds(const struct led_rgb data[]) {
	int rc = led_strip_update_rgb(strip, data, STRIP_NUM_PIXELS);
    if (rc) {
        LOG_ERR("couldn't update strip: %d", rc);
    }
}

int
main(void) {
    int led_ready = 0;
    int toggle = 0;


    if (device_is_ready(strip)) {
		LOG_INF("Found LED strip device %s", strip->name);
        led_ready = 1;
	} else {
		LOG_ERR("LED strip device %s is not ready", strip->name);
		goto END;
    }

    if (led_ready) {
		LOG_INF("Clearing LEDS...");
        set_leds(pixels_off);
		LOG_INF(".. setting startup LEDS.");
        set_leds(pixels_boot);
    }

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
        if (toggle) {
            toggle = 0;
            set_leds(pixels_payload1);
        }
        else {
            toggle = 1;
            set_leds(pixels_payload2);
        }
		k_msleep(SLEEP_TIME_MS);
    }

END:
	k_sleep(K_FOREVER);

    return 0;
}

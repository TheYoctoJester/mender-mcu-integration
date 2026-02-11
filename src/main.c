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

#ifdef CONFIG_DISPLAY
#include "utils/display.h"
#endif

#define SLEEP_TIME_MS 1000
#define FOOTER_UPDATE_INTERVAL_MS 1000

#ifdef CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE
#include <mender/zephyr-image-update-module.h>
#endif /* CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE */

#ifdef CONFIG_MENDER_APP_NOOP_UPDATE_MODULE
#include "modules/noop-update-module.h"
#endif /* CONFIG_MENDER_APP_NOOP_UPDATE_MODULE */

/* Client state tracking */
static const char *client_state = "Init";
static bool state_changed = false;

#ifdef CONFIG_DISPLAY
static void update_footer(void)
{
    char ip_buf[16];
    display_get_ip_string(ip_buf, sizeof(ip_buf));
    display_update_footer(CONFIG_MENDER_ARTIFACT_NAME, ip_buf, client_state);
}
#endif

static void set_client_state(const char *new_state)
{
    if (client_state != new_state) {
        client_state = new_state;
        state_changed = true;
        LOG_INF("Client state: %s", client_state);
#ifdef CONFIG_DISPLAY
        update_footer();
#endif
    }
}

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
deployment_status_cb(mender_deployment_status_t status, const char *desc) {
    LOG_DBG("deployment_status_cb: %s", desc);

    switch (status) {
    case MENDER_DEPLOYMENT_STATUS_DOWNLOADING:
        set_client_state("Downloading");
        break;
    case MENDER_DEPLOYMENT_STATUS_INSTALLING:
        set_client_state("Installing");
        break;
    case MENDER_DEPLOYMENT_STATUS_REBOOTING:
        set_client_state("Rebooting");
        break;
    case MENDER_DEPLOYMENT_STATUS_SUCCESS:
        set_client_state("Success");
        break;
    case MENDER_DEPLOYMENT_STATUS_FAILURE:
        set_client_state("Failed");
        break;
    default:
        break;
    }

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

int
main(void) {

#ifdef CONFIG_DISPLAY
    if (display_init() == 0) {
        display_logo();
        update_footer();
    }
#endif

    set_client_state("Net wait");
    LOG_INF("Initializing network...");
    netup_wait_for_network();

    netup_get_mac_address(mender_identity.value);

    certs_add_credentials();

#ifdef CONFIG_DISPLAY
    /* Update footer now that we have an IP */
    update_footer();
#endif

    LOG_INF("Initializing Mender Client with:");
    LOG_INF("   Device type:   '%s'", CONFIG_MENDER_DEVICE_TYPE);
    LOG_INF("   Identity:      '{\"%s\": \"%s\"}'", mender_identity.name, mender_identity.value);

    set_client_state("Starting");

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
        set_client_state("Error");
        goto END;
    }
    LOG_INF("Mender client initialized");

#ifdef CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE
    if (MENDER_OK != mender_zephyr_image_register_update_module()) {
        LOG_ERR("Failed to register the zephyr-image Update Module");
        set_client_state("Error");
        goto END;
    }
    LOG_INF("Update Module 'zephyr-image' initialized");
#endif /* CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE */

#ifdef CONFIG_MENDER_APP_NOOP_UPDATE_MODULE
    if (MENDER_OK != noop_update_module_register()) {
        LOG_ERR("Failed to register the noop Update Module");
        set_client_state("Error");
        goto END;
    }
    LOG_INF("Update Module 'noop-update' initialized");
#endif /* CONFIG_MENDER_APP_NOOP_UPDATE_MODULE */

    /* Finally activate mender client */
    if (MENDER_OK != mender_client_activate()) {
        LOG_ERR("Unable to activate the client");
        set_client_state("Error");
        goto END;
    }
    LOG_INF("Mender client activated and running!");

    set_client_state("Idle");

#ifdef CONFIG_DISPLAY
    uint32_t footer_timer = 0;
#endif

    while (1) {
        k_msleep(SLEEP_TIME_MS);

#ifdef CONFIG_DISPLAY
        /* Periodically refresh footer to catch IP changes */
        footer_timer += SLEEP_TIME_MS;
        if (footer_timer >= FOOTER_UPDATE_INTERVAL_MS) {
            footer_timer = 0;
            display_toggle_heartbeat();
            update_footer();
        }
#endif
    }

END:
    k_sleep(K_FOREVER);

    return 0;
}

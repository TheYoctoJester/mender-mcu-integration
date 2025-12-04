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
#include "display/display.h"

#include <zephyr/kernel.h>
#include <zephyr/version.h>

#include <mender/utils.h>
#include <mender/client.h>
#include <mender/inventory.h>

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

int
main(void) {
    LOG_INF("Mender MCU Integration on Zephyr %s", KERNEL_VERSION_STRING);

    display_init();

    netup_wait_for_network();

    netup_get_mac_address(get_mender_identity_value());

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
    LOG_INF("   Identity:      '{\"mac\": \"%s\"}'", get_mender_identity_value());

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

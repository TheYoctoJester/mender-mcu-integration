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

#include "utils/callbacks.h"
#include "utils/netup.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include <mender/utils.h>
#include <mender/client.h>
#include <mender/inventory.h>

LOG_MODULE_DECLARE(mender_app, LOG_LEVEL_DBG);

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

mender_err_t
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

char *get_mender_identity_value(void) {
    return mender_identity.value;
}

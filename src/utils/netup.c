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

/* This piece of code is heavily inspired by Zephyr OS project samples:
 * https://github.com/zephyrproject-rtos/zephyr/tree/v3.7.0/samples/net/dhcpv4_client
 * https://github.com/zephyrproject-rtos/zephyr/tree/v3.7.0/samples/net/cloud/tagoio_http_post
 *
 * The main entry point, netup_wait_for_network, will set up the callbacks for the network
 * management and wait for NET_EVENT_IPV4_ADDR_ADD event (iow, the device obtained an IP address.
 * If WIFI configuration is enabled, a CONNECT request is issued and it is assumed that obtaining
 * the IP address is managed somewhere else.
 * The wait from netup_wait_for_network for the described event is controlled with a semaphore. */

#include "netup.h"

#include <stdio.h>
#include <assert.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/ethernet_mgmt.h>
#include <soc/efuse_reg.h>
#if defined(CONFIG_WIFI)
#include <zephyr/net/wifi_mgmt.h>
#endif

static K_SEM_DEFINE(network_ready_sem, 0, 1);

static struct net_mgmt_event_callback mgmt_cb;

static int
set_mac_from_efuse(struct net_if *iface) {
    struct ethernet_req_params params = { 0 };
    uint8_t *mac = params.mac_address.addr;

    /* Read the base MAC directly from eFuse registers.
     * REG0 holds the low 32 bits, REG1[15:0] holds the high 16 bits. */
    uint32_t mac_low  = sys_read32(EFUSE_RD_MAC_SPI_SYS_0_REG);
    uint32_t mac_high = sys_read32(EFUSE_RD_MAC_SPI_SYS_1_REG);

    mac[0] = (mac_high >> 8) & 0xFF;
    mac[1] = (mac_high >> 0) & 0xFF;
    mac[2] = (mac_low >> 24) & 0xFF;
    mac[3] = (mac_low >> 16) & 0xFF;
    mac[4] = (mac_low >>  8) & 0xFF;
    mac[5] = (mac_low >>  0) & 0xFF;

    int ret = net_mgmt(NET_REQUEST_ETHERNET_SET_MAC_ADDRESS, iface,
                       &params, sizeof(params));
    if (ret) {
        LOG_ERR("Failed to set MAC address: %d", ret);
        return ret;
    }

    LOG_INF("MAC from eFuse: %02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return 0;
}

#if defined(CONFIG_WIFI)

static struct wifi_connect_req_params cnx_params = {
    .ssid        = CONFIG_MENDER_APP_WIFI_SSID,
    .ssid_length = strlen(CONFIG_MENDER_APP_WIFI_SSID),
    .psk         = CONFIG_MENDER_APP_WIFI_PSK,
    .psk_length  = strlen(CONFIG_MENDER_APP_WIFI_PSK),
    .channel     = 0,
    .security    = WIFI_SECURITY_TYPE_PSK,
};

static void
wifi_connect(struct net_if *iface) {
    int ret = 0;

    LOG_INF("Connecting to wireless network %s...", cnx_params.ssid);

    int nr_tries = 20;
    while (nr_tries-- > 0) {
        ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params, sizeof(struct wifi_connect_req_params));
        if (ret == 0) {
            break;
        }

        /* Let's wait few seconds to allow wifi device be on-line */
        LOG_ERR("Connect request failed %d. Waiting iface be up...\n", ret);
        k_msleep(500);
    }
}

#endif

static void
event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface) {
    int i = 0;

    if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
        return;
    }

    for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
        char buf[NET_IPV4_ADDR_LEN];

        if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type != NET_ADDR_DHCP) {
            continue;
        }

        LOG_INF("   Address[%d]: %s",
                net_if_get_by_iface(iface),
                net_addr_ntop(AF_INET, &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr, buf, sizeof(buf)));
        LOG_INF("    Subnet[%d]: %s", net_if_get_by_iface(iface), net_addr_ntop(AF_INET, &iface->config.ip.ipv4->unicast[i].netmask, buf, sizeof(buf)));
        LOG_INF("    Router[%d]: %s", net_if_get_by_iface(iface), net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, buf, sizeof(buf)));
        LOG_INF("Lease time[%d]: %u seconds", net_if_get_by_iface(iface), iface->config.dhcpv4.lease_time);
    }

    // Network is up \o/
    k_sem_give(&network_ready_sem);
}

int
netup_wait_for_network(void) {
    net_mgmt_init_event_callback(&mgmt_cb, event_handler, NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&mgmt_cb);

    /* Assume that there is only one network interface, having two or more will just pick up
    the default and and continue blindly */
    struct net_if *iface = net_if_get_default();
    LOG_INF("Using net interface %s, index=%d", net_if_get_device(iface)->name, net_if_get_by_iface(iface));

#if defined(CONFIG_WIFI)
    wifi_connect(iface);
#else
    set_mac_from_efuse(iface);

    /* For WIFI, it is expected that the dhcp client is started somehow by the network management.
    This is the case for example for ESP32-S3 with configuration option WIFI_STA_AUTO_DHCPV4 */
    net_dhcpv4_start(iface);
#endif

    // Wait for network
    LOG_INF("Waiting for network up...");
    return k_sem_take(&network_ready_sem, K_FOREVER);
}

void
netup_get_mac_address(char *address) {
    assert(NULL != address);

    struct net_if *iface = net_if_get_first_up();
    assert(NULL != iface);

    struct net_linkaddr *linkaddr = net_if_get_link_addr(iface);
    assert(NULL != linkaddr);

    snprintf(address,
             18,
             "%02x:%02x:%02x:%02x:%02x:%02x",
             linkaddr->addr[0],
             linkaddr->addr[1],
             linkaddr->addr[2],
             linkaddr->addr[3],
             linkaddr->addr[4],
             linkaddr->addr[5]);
}

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
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/net/ethernet_mgmt.h>
#include <zephyr/sys/crc.h>
#if defined(CONFIG_WIFI)
#include <zephyr/net/wifi_mgmt.h>
#endif

/* Wiznet OUI for W5500 */
#define WIZNET_OUI_B0 0x00
#define WIZNET_OUI_B1 0x08
#define WIZNET_OUI_B2 0xDC

static K_SEM_DEFINE(network_ready_sem, 0, 1);

static struct net_mgmt_event_callback mgmt_cb;

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

/**
 * Derive a unique MAC address from the ESP32 chip ID.
 *
 * Uses CRC32 hash of the full 6-byte chip ID to ensure good distribution
 * even for chips from the same production batch with sequential IDs.
 * The resulting MAC uses Wiznet's OUI (00:08:DC) for W5500 compatibility.
 */
static int set_mac_from_chip_id(struct net_if *iface)
{
    uint8_t chip_id[6];
    ssize_t len = hwinfo_get_device_id(chip_id, sizeof(chip_id));
    if (len < 6) {
        LOG_ERR("Failed to get chip ID (len=%d)", (int)len);
        return -ENODEV;
    }

    LOG_INF("Chip ID: %02x:%02x:%02x:%02x:%02x:%02x",
            chip_id[0], chip_id[1], chip_id[2],
            chip_id[3], chip_id[4], chip_id[5]);

    /*
     * Hash the full chip ID to derive 3 bytes for the MAC.
     * CRC32 provides good avalanche properties - small input changes
     * cause large output changes, avoiding collisions for sequential IDs.
     */
    uint32_t hash = crc32_ieee(chip_id, len);

    struct ethernet_req_params params;
    params.mac_address.addr[0] = WIZNET_OUI_B0;
    params.mac_address.addr[1] = WIZNET_OUI_B1;
    params.mac_address.addr[2] = WIZNET_OUI_B2;
    params.mac_address.addr[3] = (hash >> 16) & 0xFF;
    params.mac_address.addr[4] = (hash >> 8) & 0xFF;
    params.mac_address.addr[5] = hash & 0xFF;

    LOG_INF("Derived MAC: %02x:%02x:%02x:%02x:%02x:%02x",
            params.mac_address.addr[0], params.mac_address.addr[1],
            params.mac_address.addr[2], params.mac_address.addr[3],
            params.mac_address.addr[4], params.mac_address.addr[5]);

    /*
     * MAC can only be changed when interface is administratively down.
     * Zephyr auto-starts interfaces, so we need to bring it down first.
     */
    int ret = net_if_down(iface);
    if (ret < 0 && ret != -EALREADY) {
        LOG_ERR("Failed to bring interface down: %d", ret);
        return ret;
    }

    ret = net_mgmt(NET_REQUEST_ETHERNET_SET_MAC_ADDRESS, iface,
                   &params, sizeof(params));
    if (ret < 0) {
        LOG_ERR("Failed to set MAC address: %d", ret);
        /* Try to bring interface back up even if MAC setting failed */
        net_if_up(iface);
        return ret;
    }

    /* Bring interface back up */
    ret = net_if_up(iface);
    if (ret < 0) {
        LOG_ERR("Failed to bring interface up: %d", ret);
        return ret;
    }

    return 0;
}

static void
event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface) {
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

    /* Set MAC from chip ID before bringing interface up.
     * This must happen before net_dhcpv4_start() which triggers interface UP. */
    int ret = set_mac_from_chip_id(iface);
    if (ret < 0) {
        LOG_WRN("Failed to set MAC from chip ID: %d, using default", ret);
    }

#if defined(CONFIG_WIFI)
    wifi_connect(iface);
#else
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

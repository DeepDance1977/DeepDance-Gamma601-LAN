// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <string.h>

#include "driver/spi_master.h"
#include "esp_eth.h"
#include "esp_eth_mac_w5500.h"
#include "esp_eth_phy_w5500.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"

#include "ethernet.h"

#define ETHERNET_SPI_HOST SPI2_HOST

static const char * TAG = "ethernet";

static esp_eth_handle_t s_eth_handle = NULL;
static esp_netif_t * s_eth_netif = NULL;
static esp_eth_netif_glue_handle_t s_glue = NULL;
static volatile bool s_started = false;
static volatile bool s_link_up = false;
static esp_err_t s_last_error = ESP_OK;

bool ethernet_board_supported(const char * board_version)
{
    // To enable another board, first verify that GPIO 39..42 are free on it.
    return board_version != NULL && strcmp(board_version, "601") == 0;
}

static void on_eth_event(void * arg, esp_event_base_t base, int32_t id, void * data)
{
    switch (id) {
    case ETHERNET_EVENT_CONNECTED:
        s_link_up = true;
        ESP_LOGI(TAG, "Link up");
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        s_link_up = false;
        ESP_LOGI(TAG, "Link down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Driver started");
        break;
    case ETHERNET_EVENT_STOP:
        s_link_up = false;
        ESP_LOGI(TAG, "Driver stopped");
        break;
    default:
        break;
    }
}

esp_err_t ethernet_start(const char * hostname)
{
    if (s_started) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err;
    bool bus_ready = false;
    bool handler_registered = false;
    esp_eth_mac_t * mac = NULL;
    esp_eth_phy_t * phy = NULL;
    esp_eth_handle_t handle = NULL;
    esp_netif_t * netif = NULL;
    esp_eth_netif_glue_handle_t glue = NULL;

    // 1. SPI bus. If somebody else already owns SPI2 we do not take it over.
    spi_bus_config_t buscfg = {
        .mosi_io_num = ETHERNET_GPIO_MOSI,
        .miso_io_num = ETHERNET_GPIO_MISO,
        .sclk_io_num = ETHERNET_GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    err = spi_bus_initialize(ETHERNET_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
        goto fail;
    }
    bus_ready = true;

    // 2. W5500 MAC/PHY. The module has neither INT nor RST wired, so the driver
    //    must poll. ETH_W5500_DEFAULT_CONFIG() defaults int_gpio_num to GPIO 4,
    //    which must NOT be touched on the Bitaxe, hence the explicit -1.
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = ETHERNET_SPI_CLOCK_HZ,
        .spics_io_num = ETHERNET_GPIO_CS,
        .queue_size = 20,
    };
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(ETHERNET_SPI_HOST, &devcfg);
    w5500_config.int_gpio_num = -1;
    w5500_config.poll_period_ms = ETHERNET_POLL_PERIOD_MS;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = 4096;

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = -1;

    mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    phy = esp_eth_phy_new_w5500(&phy_config);
    if (mac == NULL || phy == NULL) {
        ESP_LOGE(TAG, "Could not create W5500 MAC/PHY");
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    // Driver install talks to the chip and verifies its version register, so this
    // is where a missing or miswired module is detected.
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    err = esp_eth_driver_install(&eth_config, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "W5500 not detected or driver install failed: %s", esp_err_to_name(err));
        handle = NULL;
        goto fail;
    }

    // W5500 modules have no reliable factory MAC. Use the ESP32's own (unique,
    // stable) Ethernet MAC so DHCP reservations keep working across reboots.
    uint8_t mac_addr[6];
    err = esp_read_mac(mac_addr, ESP_MAC_ETH);
    if (err == ESP_OK) {
        err = esp_eth_ioctl(handle, ETH_CMD_S_MAC_ADDR, mac_addr);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Setting MAC address failed: %s", esp_err_to_name(err));
        goto fail;
    }

    // 3. Network interface with DHCP client, preferred over Wi-Fi by route priority.
    esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_ETH();
    base_cfg.route_prio = ETHERNET_ROUTE_PRIO;
    esp_netif_config_t netif_cfg = {
        .base = &base_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };
    netif = esp_netif_new(&netif_cfg);
    if (netif == NULL) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    if (hostname != NULL && hostname[0] != '\0') {
        esp_err_t host_err = esp_netif_set_hostname(netif, hostname);
        if (host_err != ESP_OK) {
            ESP_LOGW(TAG, "esp_netif_set_hostname failed: %s", esp_err_to_name(host_err));
        }
    }

    glue = esp_eth_new_netif_glue(handle);
    if (glue == NULL) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    err = esp_netif_attach(netif, glue);
    if (err != ESP_OK) {
        goto fail;
    }

    err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &on_eth_event, NULL);
    if (err != ESP_OK) {
        goto fail;
    }
    handler_registered = true;

    err = esp_eth_start(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_eth_start failed: %s", esp_err_to_name(err));
        goto fail;
    }

    s_eth_handle = handle;
    s_eth_netif = netif;
    s_glue = glue;
    s_last_error = ESP_OK;
    s_started = true;
    ESP_LOGI(TAG, "W5500 started (SPI2 MOSI=%d MISO=%d SCLK=%d CS=%d, %d MHz, polling %d ms)",
             ETHERNET_GPIO_MOSI, ETHERNET_GPIO_MISO, ETHERNET_GPIO_SCLK, ETHERNET_GPIO_CS,
             ETHERNET_SPI_CLOCK_HZ / 1000000, ETHERNET_POLL_PERIOD_MS);
    return ESP_OK;

fail:
    s_last_error = err;
    if (handler_registered) {
        esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, &on_eth_event);
    }
    if (handle != NULL) {
        esp_eth_stop(handle);
    }
    if (glue != NULL) {
        esp_eth_del_netif_glue(glue);
    }
    if (handle != NULL) {
        // Uninstalling also deletes the MAC and PHY objects.
        esp_eth_driver_uninstall(handle);
    } else {
        if (mac != NULL) {
            mac->del(mac);
        }
        if (phy != NULL) {
            phy->del(phy);
        }
    }
    if (netif != NULL) {
        esp_netif_destroy(netif);
    }
    if (bus_ready) {
        spi_bus_free(ETHERNET_SPI_HOST);
    }
    return err;
}

bool ethernet_is_started(void)
{
    return s_started;
}

esp_err_t ethernet_last_error(void)
{
    return s_last_error;
}

esp_netif_t * ethernet_get_netif(void)
{
    return s_started ? s_eth_netif : NULL;
}

void ethernet_get_status(ethernet_status_t * out)
{
    memset(out, 0, sizeof(*out));
    out->started = s_started;
    out->link_up = s_started && s_link_up;
    if (!s_started || s_eth_netif == NULL) {
        return;
    }

    uint8_t mac[6];
    if (esp_netif_get_mac(s_eth_netif, mac) == ESP_OK) {
        snprintf(out->mac, sizeof(out->mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    esp_netif_dhcp_status_t dhcp_status;
    if (esp_netif_dhcpc_get_status(s_eth_netif, &dhcp_status) == ESP_OK) {
        out->dhcp_active = (dhcp_status == ESP_NETIF_DHCP_STARTED);
    }

    esp_netif_ip_info_t ip_info;
    if (out->link_up && esp_netif_get_ip_info(s_eth_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
        out->has_ip = true;
        snprintf(out->ip, sizeof(out->ip), IPSTR, IP2STR(&ip_info.ip));
        snprintf(out->netmask, sizeof(out->netmask), IPSTR, IP2STR(&ip_info.netmask));
        snprintf(out->gateway, sizeof(out->gateway), IPSTR, IP2STR(&ip_info.gw));
    }

    if (out->link_up && s_eth_handle != NULL) {
        eth_speed_t speed;
        eth_duplex_t duplex;
        if (esp_eth_ioctl(s_eth_handle, ETH_CMD_G_SPEED, &speed) == ESP_OK) {
            out->speed_mbps = (speed == ETH_SPEED_100M) ? 100 : 10;
        }
        if (esp_eth_ioctl(s_eth_handle, ETH_CMD_G_DUPLEX_MODE, &duplex) == ESP_OK) {
            out->full_duplex = (duplex == ETH_DUPLEX_FULL);
        }
    }
}

esp_err_t ethernet_apply_hostname(const char * hostname)
{
    if (!s_started || s_eth_netif == NULL || hostname == NULL || hostname[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_netif_set_hostname(s_eth_netif, hostname);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_netif_set_hostname failed: %s", esp_err_to_name(err));
        return err;
    }

    // Bounce the DHCP client so the new name goes out in option 12.
    err = esp_netif_dhcpc_stop(s_eth_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        return err;
    }
    err = esp_netif_dhcpc_start(s_eth_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        return err;
    }
    return ESP_OK;
}

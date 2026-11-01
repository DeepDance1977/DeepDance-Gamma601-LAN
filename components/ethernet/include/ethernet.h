// SPDX-License-Identifier: GPL-3.0-or-later
//
// W5500 SPI Ethernet support for the Bitaxe Gamma 601 (DeepDance LAN Edition).
//
// This component only brings up the physical interface (SPI bus, W5500 MAC/PHY
// driver from ESP-IDF's esp_eth, esp_netif with DHCP client). It deliberately
// contains NO network policy: preferring LAN over Wi-Fi, Wi-Fi fallback and
// pool reconnects are decided in components/connect so that there is exactly one
// network manager in the firmware.
#ifndef ETHERNET_H_
#define ETHERNET_H_

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_netif.h"

// Fixed wiring of the W5500 module on the Gamma 601 expansion header.
// Source: WillItMod/5tratumFW docs/gamma-ethernet.md (Beta 11); confirmed
// working on real hardware by the project owner.
#define ETHERNET_GPIO_MOSI        40
#define ETHERNET_GPIO_MISO        39
#define ETHERNET_GPIO_SCLK        41
#define ETHERNET_GPIO_CS          42
#define ETHERNET_SPI_CLOCK_HZ     (20 * 1000 * 1000)   // SPI mode 0, 20 MHz
#define ETHERNET_POLL_PERIOD_MS   10                    // no INT / RST pin wired

// Higher than the default route priority of the Wi-Fi station netif (100), so
// that lwIP prefers Ethernet whenever both interfaces happen to hold an address.
#define ETHERNET_ROUTE_PRIO       200

#define ETHERNET_NETIF_KEY        "ETH_DEF"

typedef struct {
    bool started;        // driver and netif are running
    bool link_up;        // PHY link detected
    bool has_ip;         // link is up and DHCP delivered an IPv4 address
    bool dhcp_active;    // DHCP client is running on the interface
    int  speed_mbps;     // 10 / 100, 0 when unknown
    bool full_duplex;
    char mac[18];        // "AA:BB:CC:DD:EE:FF"
    char ip[16];
    char netmask[16];
    char gateway[16];
} ethernet_status_t;

// True for board versions on which the W5500 pins are known to be free
// (39/40 are shared with BAP, 41/42 are unused). Currently only "601".
bool ethernet_board_supported(const char * board_version);

// Bring up SPI bus, W5500 driver and netif. Requires esp_netif_init() and the
// default event loop. Returns an error (and releases everything it allocated) if
// the module is not detected. Safe to call once per boot.
esp_err_t ethernet_start(const char * hostname);

bool ethernet_is_started(void);

// Result of the last ethernet_start() call (ESP_OK if it never failed).
esp_err_t ethernet_last_error(void);

esp_netif_t * ethernet_get_netif(void);

void ethernet_get_status(ethernet_status_t * out);

// Update the DHCP hostname without restarting (mirrors wifi_apply_hostname()).
esp_err_t ethernet_apply_hostname(const char * hostname);

#endif /* ETHERNET_H_ */

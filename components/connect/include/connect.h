#ifndef CONNECT_H_
#define CONNECT_H_

#include "lwip/sys.h"
#include <arpa/inet.h>
#include <lwip/netdb.h>
#include <stdbool.h>

#include "esp_err.h"
#include "esp_wifi_types.h"

typedef struct GlobalState GlobalState;

// Structure to hold WiFi scan results
typedef struct {
    char ssid[33];  // 32 chars + null terminator
    int8_t rssi;
    wifi_auth_mode_t authmode;
} wifi_ap_record_simple_t;

void toggle_wifi_softap(void);
void wifi_init(GlobalState * GLOBAL_STATE);
esp_err_t wifi_apply_hostname(const char *hostname);
esp_err_t wifi_scan(wifi_ap_record_simple_t *ap_records, uint16_t *ap_count);
esp_err_t get_wifi_current_rssi(int8_t *rssi);
bool wifi_is_connected(void);

typedef enum {
    NETWORK_INTERFACE_NONE = 0,
    NETWORK_INTERFACE_WIFI,
    NETWORK_INTERFACE_ETHERNET,
} network_interface_t;

// True if any uplink (Wi-Fi association or Ethernet with an address) is usable.
// Mining/stratum code must use this instead of wifi_is_connected().
bool network_is_connected(void);
network_interface_t network_get_active_interface(void);
esp_err_t update_mdns_hostname(const char *new_hostname, GlobalState *GLOBAL_STATE);

#endif /* CONNECT_H_ */

#ifndef ETHERNET_API_H
#define ETHERNET_API_H

#include "esp_http_server.h"

typedef struct GlobalState GlobalState;

// Register GET /api/system/ethernet (read-only status of the W5500 interface and
// the active uplink). Enabling/disabling is done via PATCH /api/system with
// {"ethernetEnabled": 0|1}; the change takes effect after a restart.
esp_err_t register_ethernet_api_endpoints(httpd_handle_t server, void * ctx, GlobalState * global_state);

#endif // ETHERNET_API_H

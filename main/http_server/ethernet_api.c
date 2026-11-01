// SPDX-License-Identifier: GPL-3.0-or-later
#include "ethernet_api.h"

#include <string.h>

#include "cJSON.h"
#include "connect.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "ethernet.h"
#include "global_state.h"
#include "http_server.h"
#include "nvs_config.h"

static const char * TAG = "ethernet_api";

static GlobalState * s_global_state = NULL;
static int ethernet_prebuffer_len = 512;

static const char * interface_name(network_interface_t iface)
{
    switch (iface) {
    case NETWORK_INTERFACE_ETHERNET:
        return "ethernet";
    case NETWORK_INTERFACE_WIFI:
        return "wifi";
    default:
        return "none";
    }
}

static esp_err_t ethernet_get_handler(httpd_req_t * req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    set_cors_headers(req);

    ethernet_status_t st;
    ethernet_get_status(&st);

    const bool supported = ethernet_board_supported(s_global_state->DEVICE_CONFIG.board_version);
    const bool enabled = nvs_config_get_bool(NVS_CONFIG_ETHERNET_ENABLED);
    const esp_err_t start_err = ethernet_last_error();

    cJSON * root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "supported", supported);
    cJSON_AddBoolToObject(root, "enabled", enabled);
    cJSON_AddBoolToObject(root, "active", st.started);
    cJSON_AddBoolToObject(root, "restartRequired",
                          (enabled && supported && !st.started && start_err == ESP_OK) || (!enabled && st.started));
    cJSON_AddStringToObject(root, "error", (enabled && !st.started && start_err != ESP_OK) ? esp_err_to_name(start_err) : "");

    cJSON_AddBoolToObject(root, "linkUp", st.link_up);
    cJSON_AddBoolToObject(root, "connected", st.has_ip);
    cJSON_AddBoolToObject(root, "dhcp", st.dhcp_active);
    cJSON_AddStringToObject(root, "ipv4", st.ip);
    cJSON_AddStringToObject(root, "netmask", st.netmask);
    cJSON_AddStringToObject(root, "gateway", st.gateway);
    cJSON_AddNumberToObject(root, "speedMbps", st.speed_mbps);
    cJSON_AddBoolToObject(root, "fullDuplex", st.full_duplex);
    cJSON_AddStringToObject(root, "mac", st.mac);

    cJSON_AddBoolToObject(root, "wifiConnected", wifi_is_connected());
    cJSON_AddStringToObject(root, "wifiStatus", s_global_state->SYSTEM_MODULE.wifi_status);
    cJSON_AddStringToObject(root, "activeInterface", interface_name(network_get_active_interface()));

    esp_err_t res = HTTP_send_json(req, root, &ethernet_prebuffer_len);
    cJSON_Delete(root);
    return res;
}

esp_err_t register_ethernet_api_endpoints(httpd_handle_t server, void * ctx, GlobalState * global_state)
{
    s_global_state = global_state;

    httpd_uri_t ethernet_get = {
        .uri = "/api/system/ethernet",
        .method = HTTP_GET,
        .handler = ethernet_get_handler,
        .user_ctx = ctx,
    };

    esp_err_t err = httpd_register_uri_handler(server, &ethernet_get);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not register /api/system/ethernet: %s", esp_err_to_name(err));
    }
    return err;
}

# Integration notes (Änderungen gegenüber ESP-Miner v2.15.4)

Baseline: commit "Initial ESP-Miner 2.15.4 base". Everything below is the complete delta.

## New files
| File | Purpose |
|---|---|
| `components/ethernet/{CMakeLists.txt,idf_component.yml,ethernet.c,include/ethernet.h}` | SPI2 + W5500 (`esp_eth`, registry component `espressif/w5500 ^1.0.1`), netif ETH_DEF with DHCP, status. No policy. |
| `main/http_server/ethernet_api.{c,h}` | `GET /api/system/ethernet` |
| `main/http_server/axe-os/src/app/components/ethernet/*` | AxeOS page "LAN / Ethernet" |
| `.github/workflows/build-lan.yml` | Build + factory image + checksums + pre-release on `v*` tags |
| `README.de.md`, `docs/*` | Documentation |

## Modified files (re-apply these on an upstream update)
| File | Change |
|---|---|
| `main/nvs_config.{h,c}` | New key `eth_enabled` / REST `ethernetEnabled` (bool, default false). Exposed through the generic `PATCH /api/system`. |
| `main/device_config.c` | After `device_pins_init()`: if Ethernet enabled and board supported, `pins.bap = NULL` (frees GPIO 39/40). |
| `components/connect/connect.{c,h}`, `CMakeLists.txt` | Uplink policy: Ethernet events, LAN preferred (Wi-Fi disconnected), Wi-Fi fallback on link-down, guards in the STA handlers, `network_is_connected()`, `network_get_active_interface()`, active-netif for mDNS IP lookups, Ethernet hostname sync. |
| `main/tasks/protocol_coordinator.{c,h}` | New event `COORD_EVENT_NETWORK_CHANGED` / `protocol_coordinator_notify_network_change()`: restarts the running protocol task on the same pool (no failure counted). |
| `main/tasks/stratum_v1_task.c`, `stratum_v2_task.c`, `protocol_coordinator.c` | `wifi_is_connected()` -> `network_is_connected()` (5 call sites). |
| `main/http_server/http_server.c` | `max_uri_handlers` 25 -> 28 (upstream already registered exactly 25), register the new endpoint. |
| `main/http_server/openapi.yaml` | Endpoint, `EthernetStatus` schema, `ethernetEnabled` in `Settings`. |
| `main/CMakeLists.txt` | New source file, `ethernet` in REQUIRES. |
| AxeOS: `app.module.ts`, `app-routing.module.ts`, `layout/app.menu.component.ts`, `services/system.service.ts` | Route, declaration, menu entry, `getEthernetStatus()`. |
| `.github/workflows/` | Upstream build/release/docker workflows removed (need upstream secrets); `unittest*.yml` kept. |

## Design decisions
- Policy only in `connect.c` (single network manager). The ethernet component never touches GlobalState.
- Loss signal is `ETHERNET_EVENT_DISCONNECTED` (link-down), not `IP_EVENT_ETH_LOST_IP`, which esp_netif delays by its IP-lost timer.
- `int_gpio_num = -1` is mandatory: the W5500 default config would otherwise claim GPIO 4.
- MAC is derived from the ESP32 base MAC (`ESP_MAC_ETH`); W5500 modules have no dependable factory MAC.
- The Wi-Fi STA disconnect handler blocks the default event loop for 5 s while retrying (upstream behaviour); Ethernet events can be delayed by that long while Wi-Fi is failing.

## Not verified yet (no compiler/hardware in the authoring environment)
- Compilation against ESP-IDF 6.0.2 and `espressif/w5500` 1.x (struct/field names follow the registry README and the ESP-IDF example API).
- Angular build with the regenerated API client.
- Behaviour on hardware, `.local` mDNS over Ethernet, route preference when both interfaces hold an address.

# DeepDance Gamma 601 LAN Edition

Modifizierte [ESP-Miner](https://github.com/bitaxeorg/ESP-Miner)-Firmware (Basis **v2.15.4**, Commit `d212a0f`) für den **Bitaxe Gamma 601** mit zusätzlicher Unterstützung eines **W5500-Ethernet-Moduls** (SPI). Die originale AxeOS-Oberfläche bleibt erhalten und wurde nur um die Seite **LAN / Ethernet** erweitert.

> **Teststatus (bitte ernst nehmen):** Der Code ist geschrieben, aber zum Zeitpunkt dieser Dokumentation weder in GitHub Actions gebaut noch auf Hardware getestet. Siehe [docs/TESTPLAN.de.md](docs/TESTPLAN.de.md). Erst flashen, wenn der Build grün ist **und** die Originalfirmware gesichert wurde.

## Funktionen

- W5500 über SPI2, DHCP, Verbindungsstatus
- **LAN hat Vorrang**: Sobald Ethernet eine IP hat, wird WLAN getrennt (Zugangsdaten bleiben gespeichert), damit es genau einen aktiven Uplink gibt
- **WLAN-Fallback**: Bei Link-Verlust wird WLAN sofort wieder verbunden; kommt LAN zurück, übernimmt es wieder
- Bei jedem Wechsel (und bei geänderter DHCP-Adresse) wird die **Pool-Verbindung kontrolliert neu aufgebaut**, ohne dass das als Pool-Ausfall gezählt wird und ohne den Pool zu wechseln
- Ethernet ist **standardmäßig aus** und wird in AxeOS oder per API aktiviert (Neustart nötig)
- Keine zusätzlichen Netzwerkports, keine Änderung an ASIC-Steuerung, Mining-Logik, Partitionstabelle oder Build-Konfiguration

## Verdrahtung (Gamma 601)

| W5500-Signal | ESP32-S3 | Hinweis |
|---|---|---|
| MOSI | GPIO 40 | |
| MISO | GPIO 39 | |
| SCLK | GPIO 41 | |
| CS / SCS | GPIO 42 | |
| INT, RST | nicht verbunden | Treiber arbeitet im **Polling** (10 ms) |
| GND | GND | |
| VCC | laut Datenblatt **deines** Moduls | siehe Warnung |

SPI2, Mode 0, 20 MHz. Quelle der Pinbelegung: Dokumentation von WillItMod/5tratumFW (Beta 11); sie wurde vom Projektinhaber mit dem WillItMod-Image auf echter Hardware bestätigt.

**Warnung Spannung:** Der W5500-Chip arbeitet mit 3,3 V. Ob dein Modul 3,3 V oder 5 V an VCC erwartet, hängt vom Modul ab (Regler auf der Platine?). Die ESP32-Pins sind **nicht 5-V-tolerant**. Datenblatt des Moduls prüfen, bevor du etwas anschließt.

**Pin-Konflikt BAP:** GPIO 39/40 nutzt auch die BAP-Schnittstelle (UART). Ist Ethernet aktiviert, wird **BAP automatisch deaktiviert** (über den vorhandenen Mechanismus `pins.bap = NULL`). Ethernet startet außerdem nicht, wenn I2C, LCD, ASIC-Steuerung oder Tasten einen der vier Pins belegen.

## Installation

Alle Dateien liegen im Release bzw. als Workflow-Artefakt `deepdance-gamma601-lan-firmware`:

| Datei | Zweck |
|---|---|
| `esp-miner.bin` | Update-Image, **empfohlen**. Behält alle Einstellungen. |
| `esp-miner-factory-601-lan-<Version>.bin` | Komplett-Image **mit** Gamma-601-Werkseinstellungen (setzt Einstellungen zurück) |
| `esp-miner-merged-601-lan-<Version>.bin` | Komplett-Image ohne Werkseinstellungen |
| `SHA256SUMS.txt` | Prüfsummen (`sha256sum -c SHA256SUMS.txt`) |

**Vor dem ersten Flash: Original sichern** (Gamma hat 16 MB Flash):

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 read_flash 0x0 0x1000000 gamma601-backup.bin
```

**Weg 1 (empfohlen): OTA.** AxeOS → *Update* → `esp-miner.bin` hochladen. Der Miner startet neu, alle Einstellungen bleiben.

**Weg 2: Seriell** (leeres oder wiederherzustellendes Gerät):

```bash
esptool.py --chip esp32s3 -p <PORT> write_flash 0x0 esp-miner-factory-601-lan-<Version>.bin
```

## Konfiguration

1. AxeOS → **LAN / Ethernet** → Haken *Enable W5500 Ethernet module* → *Save* → *Restart now*.
2. Oder per API: `PATCH /api/system` mit `{"ethernetEnabled": 1}` und anschließend `POST /api/system/restart`.

Die Seite zeigt: Ethernet aktiviert/deaktiviert, Link, Geschwindigkeit/Duplex (falls ermittelbar), LAN-Status, DHCP, IPv4, Subnetzmaske, Gateway, MAC, WLAN-Status und die aktuell genutzte Verbindung. Die WLAN-Konfiguration unter *Network* funktioniert unverändert.

### API

`GET /api/system/ethernet` (gleiche Zugriffsprüfung wie die übrige AxeOS-API, dokumentiert in `main/http_server/openapi.yaml`):

| Feld | Bedeutung |
|---|---|
| `supported` / `enabled` / `active` | Board unterstützt es / gespeichert / seit dem Boot gestartet |
| `restartRequired` | gespeicherter Zustand weicht vom laufenden ab |
| `error` | Startfehler, z. B. Modul nicht erkannt (sonst leer) |
| `linkUp`, `connected`, `dhcp` | Link, IPv4 vorhanden, DHCP-Client läuft |
| `ipv4`, `netmask`, `gateway`, `mac` | Adressdaten |
| `speedMbps`, `fullDuplex` | Linkgeschwindigkeit (0 = unbekannt) |
| `wifiConnected`, `wifiStatus` | WLAN-Zustand |
| `activeInterface` | `ethernet`, `wifi` oder `none` |

## Wiederherstellung

1. **OTA zurück:** die Original-`esp-miner.bin` von ESP-Miner v2.15.4 über AxeOS → *Update* einspielen.
2. **Seriell:** Backup oder `esp-miner-factory-601-v2.15.4.bin` mit `write_flash 0x0 …` schreiben.
3. Ethernet lässt sich auch ohne Neuflashen abschalten: *LAN / Ethernet* → Haken entfernen → Restart. BAP ist danach wieder aktiv.
4. Ist das Gerät nicht erreichbar: Bootloader-Modus (BOOT halten, Strom anstecken) und Weg 2.

## Bekannte Einschränkungen

- Nur **Gamma 601** (Board-Version `601`). Weitere Boards erst nach Prüfung der Pins in `ethernet_board_supported()` freigeben.
- Aktivieren/Deaktivieren wirkt erst nach Neustart. BAP ist bei aktivem Ethernet aus.
- Hat der LAN-Link eine IP, aber kein Internet, wechselt die Firmware **nicht** automatisch auf WLAN. Fallback greift bei Link-Verlust.
- Beim Wechsel auf Ethernet wird WLAN absichtlich getrennt; das WLAN-Signal in der Kopfzeile zeigt dann keinen sinnvollen Wert.
- IPv6-Link-Local-Pooladressen nutzen weiterhin die WLAN-Schnittstelle für die Scope-ID (selten relevant).
- Ob `<hostname>.local` (mDNS) über LAN funktioniert, ist ungeprüft.
- Die Registry-Komponente `espressif/w5500` ist auf `^1.0.1` festgelegt. Nach dem ersten grünen Build `dependencies.lock` einchecken, damit Builds reproduzierbar bleiben.

## Entwicklung und Updates auf neue ESP-Miner-Versionen

Der Build läuft über `.github/workflows/build-lan.yml` (ESP-IDF v6.0.2, Node 24.14.0, wie upstream). Ein Tag `v*` erzeugt zusätzlich ein **Pre-Release** mit Prüfsummen. Alle geänderten und neuen Dateien sind in [docs/INTEGRATION_NOTES.md](docs/INTEGRATION_NOTES.md) aufgelistet. Für ein Upstream-Update reicht es, genau diese Stellen neu anzuwenden.

## Lizenz und Herkunft

GPL-3.0 wie ESP-Miner (siehe `LICENSE`). Basis: bitaxeorg/ESP-Miner v2.15.4. Aus WillItMod/5tratumFW wurde **kein Code** übernommen, nur die dokumentierte Pinbelegung und das Verhaltenskonzept als Referenz. Der Ethernet-Treiber ist der Standardtreiber `esp_eth`/W5500 aus der Espressif Component Registry.

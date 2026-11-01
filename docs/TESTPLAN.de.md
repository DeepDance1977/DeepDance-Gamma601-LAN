# Testplan und Teststatus

Status: **offen** = nicht ausgeführt. Nichts hier ist bisher bestätigt.

## Build (GitHub Actions)

| Test | Status |
|---|---|
| Firmware kompiliert fehlerfrei (ESP-IDF v6.0.2) | offen |
| Registry-Komponente `espressif/w5500 ^1.0.1` wird aufgelöst und passt zur API | offen |
| AxeOS inkl. neuer Seite gebaut und eingebettet; API-Client enthält `getEthernetStatus` | offen |
| `tools/test_ca_bundle.py` besteht | offen |
| Keine Pin-Konflikte (Log: kein `PIN CONFLICT` bei Ethernet an/aus) | offen |

## Netzwerk (Hardware)

| Test | Status |
|---|---|
| Start mit LAN: DHCP-IP, AxeOS per LAN erreichbar | offen |
| Pool per LAN erreichbar, Shares werden angenommen | offen |
| Start ohne LAN, mit WLAN | offen |
| Ethernet aktiviert, aber ohne Modul: läuft normal mit WLAN weiter, `error` gesetzt | offen |
| Kabel ziehen im Betrieb: WLAN-Fallback, Pool-Session neu | offen |
| Kabel stecken: Rückwechsel auf LAN | offen |
| 20 Wechsel hintereinander stabil, keine Neustarts | offen |
| Ethernet ausgeschaltet: Verhalten wie Original 2.15.4, BAP aktiv | offen |
| Ethernet ohne WLAN-Konfiguration (nur LAN) | offen |

## Mining (Hardware)

ASIC startet, Hashrate stabil, Temperatur/Lüfter/Spannung/Strom plausibel, Shares angenommen, Pool stabil, keine unerwarteten Neustarts (je 24 h): **offen**

## Oberfläche

Originales Design unverändert, Menüpunkt *LAN / Ethernet*, Anzeigen korrekt, Speichern + Neustart, WLAN-Einstellungen unverändert: **offen**
